# Verification

What was actually checked, how, and what wasn't. Keep the distinction — it matters for the
report, where "verified" and "compiles" are very different claims.

## 1. Shader validation — `tint`

Dawn ships a WGSL validator, already vendored:

```
extern/dawn/install/Release/bin/tint
```

The kernels use weBIGeo's `///use` include directive, which `tint` doesn't understand, so
the includes have to be resolved first. Script is in the session scratchpad
(`validate_wgsl.py`): it inlines `///use name` from `webgpu/compute/shaders/` and
`///use webgpu::name` from `webgpu/base/shaders/`, strips other `///` directives, then runs
`tint --format wgsl` on each kernel.

All 8 kernels pass. Worth re-running after any shader edit — much faster than launching the
app, and WGSL otherwise only fails at runtime.

## 2. SVD — checked against numpy

The 3×3 SVD is the highest-risk piece: a bug there produces plausible-looking garbage rather
than an obvious failure. Ported **verbatim** to Python (same scalar formulas, same rotation
order) and tested against numpy:

| Case | Result |
|---|---|
| Identity, pure rotation, uniform compression, anisotropic | exact reconstruction |
| **Inverted** (det F < 0) | correct — signed σ₃ reproduces it |
| Near-degenerate (σ₃ ≈ 1e-7) | stable, orthonormal U/V |
| 200 near-identity + 500 fully random matrices | max reconstruction error < 1e-4 |

All cases: `U Σ Vᵀ = F`, U and V orthonormal with det = +1.

## 3. MPM loop — offline port

The plan's Phase 2 ("de-risk the algorithm outside WGSL"), done after the fact rather than
before. P2G / grid update / G2P ported to Python with the same normalised-mass convention
and a flat floor instead of a DEM.

Results over 900 substeps, 150 particles:

| Check | Result |
|---|---|
| Mass conservation | **exact** (150.00 throughout) |
| Free fall | v = 5.886 m/s after 0.6 s — matches `g·t` to 3 decimals |
| Impact and settling | 5.98 → 1.48 → 0.19 → 0.06 m/s, comes to rest |
| Penetration | min z = 3.00 exactly — rests on the floor, no sinking |
| Deposit shape | mean z stabilises at 4.00 — a pile, not a collapsed plane |
| **Plastic compaction** | mean `jp` (now `plastic_state`) 1.0 → 0.746 on impact |

That last row is the important one: the snow **permanently densifies on impact instead of
bouncing back elastically**. That is the Stomakhin model doing its job, and it is the
qualitative behaviour the proposal was after.

The free-fall result is a strong signal for the transfer chain specifically — reproducing
`g·t` exactly means mass-weighted P2G and G2P round-trip without loss.

## 4. Runtime — real device

Booted with the MPM graph as the startup preset and confirmed (via a temporary `qInfo`) that
all 8 shaders compiled and all 8 compute pipelines were created on Metal with **no Dawn
validation errors**. Temporary changes reverted.

Note `register_shader` / `register_bind_group_layout` / `register_pipeline` build
**immediately** when a device already exists, so constructing the node is what compiles the
shaders — loading the graph is a real test, not just a parse.

## 4b. Refactor safety — resolved-shader diff against HEAD

For the 2026-09-13 dispatcher refactor (`snow_stress` → `mpm_material_stomakhin`, `jp` →
`plastic_state`, friction split into `mpm_friction`), "behaviour-preserving" was checked
rather than assumed. `scripts/check_refactor_preserving.py` resolves the `///use` includes
for each kernel from **git HEAD** and from the working tree, extracts every top-level
definition, normalises the intentional renames inside each body, folds the dispatcher
wrappers away, and diffs what is left.

Result: every retained function body identical modulo renames. The only definitions that
disappeared from a kernel were ones it never called (the old `mpm_common` injected the
material and collision code into *every* kernel, including `mpm_prepare` and `mpm_splat`),
verified by checking each against that kernel's `computeMain`. `MpmSettings` gained exactly
the four expected fields.

A Python port cannot show this — it only shows the *maths* is unchanged. Diffing the
flattened WGSL shows the *code* is. Both were done.

## 4c. Friction laws — closed-form slope mechanics

`scripts/test_friction.py` ports `mpm_friction.wgsl` verbatim and steps a point mass on an
inclined plane exactly the way `mpm_grid_update` does (gravity, then contact response).
Added with Voellmy on 2026-09-13; it also pins the Coulomb path.

| Check | Result |
|---|---|
| Voellmy terminal velocity, 35°, μ 0.155, ξ 4000, h 1.5 | 51.72 m/s vs analytic `√(ξh(sinθ−μcosθ))` = 51.77 — **0.085%** (explicit-Euler lag) |
| Coulomb slope acceleration, same slope | 4.381 m/s² vs analytic `g(sinθ−μcosθ)` = 4.381 — exact to 3 dp |
| Below atan(μ), both models | stick, final speed exactly 0 |
| `apply_basal_drag = false` | reduces **exactly** to Coulomb — the particle-level call is unchanged |
| Reversal guard | 500 m/s with dt = 1 s → 91 m/s, never negative |

The Coulomb row is the regression check for the refactor; the terminal-velocity row is the
only property that distinguishes Voellmy from Coulomb, so it is the one that matters.

## 4d. Drucker–Prager — return mapping vs the yield surface

`scripts/test_material_dp.py` ports `mpm_material_drucker_prager.wgsl` verbatim. The yield
function `y(τ) = ‖dev τ‖ + α tr τ` is closed-form, so every projection can be checked
against it directly:

| Check | Result |
|---|---|
| F = I | zero stress, Case I |
| **Pure hydrostatic compression** | Case I, unchanged — *the first draft got this wrong* |
| Hydrostatic expansion | Case II, F → I, plastic strain > 0 |
| Small shear under compression | y < 0, Case I, unchanged |
| Large shear under compression | Case III, and the projected stress satisfies **\|y\| = 1.8e-12** |
| Idempotence | re-projecting an on-cone state moves it by < 1e-9 |
| 500 random gradients | never outside the cone (max y 1.5e-10); every Case III lands on it |
| Stress symmetry / rotation covariance | τ symmetric; `τ(RF) = R τ(F) Rᵀ` |

`test_mpm.py` now takes `MPM_MODEL=drucker_prager`. Same drop test, side by side:

| | Stomakhin | Drucker–Prager |
|---|---|---|
| mean z at 900 steps | 4.00, stable since step 450 | 3.65, still falling |
| max \|v\| at 900 steps | 0.06 m/s, at rest | 2.7 m/s, still spreading |
| plastic state | saturates at 0.746 | grows without bound, 0.25 → 0.92 |

Cohesive snow piles and stops; cohesionless material keeps yielding and spreading. That is
the expected physical difference, and it falls out of the ports without any tuning. Mass
conserved exactly in both; nothing below the floor.

## 4e. Cohesive Cam Clay — return mapping, hardening law, Li 2021 parameters

`scripts/test_material_ccc.py` ports `mpm_material_ccc.wgsl` verbatim. Twelve checks:

| Check | Result |
|---|---|
| `p₀(initial α) == p₀ⁱⁿⁱ` | exact (α₀ = −19.28 at Case V's ξ = 0.002) |
| Inside ellipse | elastic, unchanged |
| **Shear (Case 3)** | projected onto the ellipse, `\|y\| ≈ 7e-8` on a 1e7 scale, **at the same p**, no hardening |
| **Compressive cap (Case 1)** | returns to `(p₀, 0)` exactly; α down, p₀ 3000 → 3012 |
| **Tensile tip (Case 2)** | returns to `(−βp₀, 0)` exactly; α up, p₀ 3000 → 2997.6 |
| 500 random gradients | never outside the surface they were projected onto |
| Repeated tension, ξ = 1 | p₀ monotone down to **exactly 0** — fracture |
| Repeated compression | p₀ monotone up 3000 → 4716, then stops once admissible |
| β = 0 | no tensile strength — any tension is Case 2 |
| **Li 2021 Table 1, all five cases** | finite α₀, p₀ round-trips at E = 3 MPa, ν = 0.3 |
| Stress symmetry / rotation covariance | holds |

Two of these failed on the first run because the test's strains were too small to reach
the yield surface (0.2 % expansion gives p = −467 Pa against a 600 Pa tensile strength) —
the loops were correctly elastic. Test inputs, not the model.

`test_mpm.py` with `MPM_MODEL=ccc`, same drop:

| | Stomakhin | Drucker–Prager | CCC Case V (p₀ 3 kPa) | CCC Case III (p₀ 42 kPa) |
|---|---|---|---|---|
| mean z | 4.00 | 3.65, falling | **3.02** — pancake | **4.13** — pile |
| max \|v\| | 0.06, at rest | 2.7, spreading | 0.05, at rest | 1.3, settling |

Weak snow flattens on impact (14 kPa ≫ 3 kPa cap — Cam-Clay loses shear strength above
p₀, the opposite of the cone), strong snow piles like Stomakhin. Same code, four parameters
changed, behaviour moves the way Li et al. describe. Both CCC cases come to rest; DP does not.

## 4f. Diagnostics readback — end to end on real terrain

The async GPU→CPU readback is a path neither `tint` nor the offline ports touch, so it was
checked by temporarily auto-running the graph on load and logging what came back
(Breite Ries preset, 24 substeps × 0.01 s = 0.24 s simulated):

| Value | Read back | Sanity |
|---|---|---|
| active particles | 131072 | all seeded (`seed_anywhere` on in the preset) |
| plastic particles | 968 (0.7 %) | slab barely moving yet; only edges have yielded |
| max speed | 1.57 m/s | `g·sinθ·t` on a ~35° slope ≈ 1.4 m/s |
| terrain range | 1303–2060 m | Schneeberg domain; track spans 1364–1931 m |

Every number is physically plausible, and it is the first real-terrain diagnostic the solver
has ever produced. Temporary changes reverted.

## 4g. Energy-line test — force balance of the coupled loop

The validation com1DFA uses (Tonnel et al. 2023 §5.2). Along the centre-of-mass path the
energy height `h_E = z + v̄²/2g` drops by exactly μ per horizontal metre under Coulomb
friction, independent of slope geometry; so the slope of `h_E` vs horizontal distance is
`−μ_eff`, and `μ_eff − μ` is internal dissipation.

`scripts/test_energy_line.py` — a slab on a 30° plane, 60 particles, 3.2 s, Stomakhin:

| Run | Travelled | `h_E` change | `μ_eff` |
|---|---|---|---|
| μ = 0.0 | 21.2 m (87 % of free slide) | 34.79 → 34.72 m | **0.0026** |
| μ = 0.3 | 10.2 m (87 % of free slide) | 34.79 → 31.65 m | **0.3050** |
| difference | | | **0.3024** vs 0.3 expected |

The frictionless run loses 7 cm of energy height over 21 m — the loop conserves energy.
The basal contribution is recovered to under 1 %. Both runs reach the same 87 % of the
ideal free-slide distance, so that shortfall is seeding/startup geometry, not friction.
This is the strongest single validation in this document: every stage of the loop and the
boundary condition, balanced together.

The first version of the test *failed*, informatively: the terrain rises with +x so
downhill is −x, and the slab was seeded 2–7 m from the domain wall. It slid into the wall
and reported `μ_eff = 0.91` on a frictionless plane. Test geometry, fixed by seeding at the
top of the slope.

**On device** (temporary auto-play, Breite Ries preset, 12 runs = 2.9 s): path 0 → 4.9 m,
`h_E` 1950.2 → 1946.9 m, `v̄²` 0.74 → 30.1 m²/s², `μ_eff` 0.65 → 0.61 and converging;
~0.59 over the last window against μ = 0.47. That ~0.12 excess is internal dissipation on
real non-planar terrain — the smooth plane gave 0.003.

## 4h. Fixed-point accumulation at device scale — `test_sheet_fixed_point.py`

A vectorised (numpy) copy of the loop with the P2G quantisation emulated per contribution:
a 1.5 m slab in 12.5 m cells on a 35° plane, μ = 0.47, Stomakhin, 20 s. The rigid-block
answer is `a = g(sin θ − μ cos θ) = 1.85 m/s²`.

| Scheme | mean speed at 20 s | vs block |
|---|---|---|
| floats | 37.00 m/s | 0.0 % |
| shader: round, momentum 1e-4, mass 2⁻²⁰ | 36.97 m/s | −0.1 % |
| old: truncate, both 1e-4 | 161 m/s, node velocities > 1000 m/s | +336 % |

Passes when floats and the shader scheme are within 3 % of the block. This is the
regression test for bug 8; it is the only offline check that exercises the integer
arithmetic, so run it after touching `to_fixed`, `add_node_mass` or the scales.

## 4i. Terrain-following grid — band vs dense

Two checks that the band grid is a memory layout, not a physics change:

- Offline, `test_sheet_fixed_point.py` also runs the float reference with the band
  emulated (8 layers per column, nodes outside skipped): identical to the dense grid to
  the printed precision (37.00 / 37.04 m/s at 20 s).
- On device, the Breite Ries run (Stomakhin preset, 131 k particles, 144 s) before and
  after: dense 128³ over a 1.6 km box vs band 320² × 16 over a 4 km box at the same
  `dx = 12.5 m`. Path 380.7 vs 378.3 m, final centre-of-mass altitude 1756.5 vs 1757.6 m,
  `μ_eff` 0.4903 vs 0.4904, max speed 21–26 m/s in both. The remaining difference is the
  different domain origin (particles fall on different sub-cell positions). Run time
  116 → 100 ms for 6× the area.

## 5. On real terrain

Runs end to end on the Schneeberg DEM: seeds, flows downhill, deposits, animates, and the
domain-centre readout matches the intended coordinates.

**Not quantitatively validated.** No comparison against a real avalanche, a reference
implementation, or measured runout. The proposal is explicit that this is educational, and
that should stay explicit in the report.

## Bugs found and fixed along the way

Worth keeping — they are the interesting part of the implementation story.

1. **SIGSEGV on any transport control before a full graph run.** `is_socket_connected()`
   returning true doesn't mean the payload exists — an output socket returning
   `unique_ptr::get()` is null until its node has run. `rerun()` re-runs a single node with
   the last buffered context, so the UI could reach `run_impl()` with a null height texture.
   Fixed with `has_valid_inputs()` (checks connectivity *and* non-null) plus disabled UI.

2. **Invisible output.** Single-texel splatting at 1 m/texel put 65 k particles into ~6% of
   a million texels at count 1. Rendering correctly, invisible in practice. Fixed by
   splatting discs and deriving the density reference instead of hard-coding it.

3. **Animation stopping.** Stepping was driven from the node editor's settings panel, which
   only renders while that editor is open *and* the node selected. Moved to
   `AvalanchePanel::draw()`, which runs every frame regardless of panel visibility.

4. **Normalized coordinates drifting.** Domain/release positions as region fractions moved
   relative to the ground whenever tile snapping changed the region. Switched to lat/lon,
   converted where the region bounds are known.

5. **`qDebug()` is filtered** in this app's logger — use `qInfo()` for anything that needs to
   show up in the console. Cost an entire debugging round to notice.

7. **Quantised GPU reduction.** Summing `value/N × scale` to keep a mean in an `i32`
   quantises each particle to `floor(v²/N × 10⁴)` — zero below 3.6 m/s at N = 131072.
   Kinetic energy read 0 for the first three runs and ~30 % low after. Found by looking
   at the raw samples, not by any test. Replaced with 64-bit lo/hi sums with carry
   detection from `atomicAdd`'s return value.

8. **The simulation never cools down** (found by the energy-line readout: `μ_eff` fell to
   0.03 and negative while the basal μ was 0.47; max speed 17 000 m/s after two minutes).
   Symptoms that mattered: model-independent, particle-count-independent, per-step energy
   injection *independent of dt* (a smaller dt blew up sooner), all particles within one
   cell of the surface, and dropping the APIC `C` from P2G made it go away. That last one
   was a red herring — `C` was the amplifier, not the source. Every offline plane test was
   perfect because they ran at `dx = 1` with a 1.5 m slab, where no stencil weight is
   small. Re-running the plane at device scale (`dx = 12.5`, slab 0.12 cells thick) was
   still perfect in floats, and reproduced the blow-up the moment the shader's fixed-point
   truncation was emulated: truncating `i32(w · 1e4)` loses relatively more of a small
   mass contribution than of its momentum, so `momentum / mass` is biased high at every
   low-weight node, and APIC feeds it back. Fixed by rounding both accumulators and moving
   the mass to a 64-bit `u32` pair at 2⁻²⁰ (§4h). On device afterwards: max speed 21–26 m/s
   for the whole run, `μ_eff` 0.58 → 0.49 against a basal 0.47, flow stops at 340–380 m.
   Lesson for the report: **verify the numerics at the production scale, including the
   integer arithmetic** — the maths port had passed everything.

   Detours that were ruled out along the way, so nobody repeats them: particle-level speed
   clamps, a strain-rate limit on the F update, a contact band / two-sided terrain BC,
   smoothed terrain normals, CFL (dt = 0.005 was worse), RPIC (rotation-only C) and a cap
   on `|C·dpos|` — the last two delayed the runaway without removing it.

6. **Switching the material model without reseeding.** `plastic_state` is model-specific.
   Stomakhin's `Jp = 1` read as Cam-Clay's `α = 1` is a fully softened material with
   `p₀ = 0` that carries no stress. Found while adding presets; both panels now force a
   reset on any model change.

## Re-running the checks

Scripts are in [`scripts/`](scripts/) — they are the evidence behind every claim on this
page, so they live with the docs rather than in a scratchpad.

```bash
cd mpm-mls-doc/scripts

# 1. shader validation (no dependencies, needs a built extern/dawn)
python3 validate_wgsl.py

# 2 + 3. offline maths (needs numpy)
python3 -m venv .venv && ./.venv/bin/pip install numpy
./.venv/bin/python test_svd.py
./.venv/bin/python test_mpm.py      # slow, pure Python: ~900 substeps
./.venv/bin/python test_friction.py # basal friction laws vs closed-form slope mechanics
./.venv/bin/python test_material_dp.py               # Drucker-Prager return mapping vs yield surface
./.venv/bin/python test_material_ccc.py              # Cam-Clay return mapping, hardening, Li 2021 params
MPM_MODEL=drucker_prager ./.venv/bin/python test_mpm.py   # full loop with DP
MPM_MODEL=ccc ./.venv/bin/python test_mpm.py              # full loop with CCC
./.venv/bin/python test_energy_line.py               # force balance of the coupled loop (~3 min)
./.venv/bin/python test_sheet_fixed_point.py         # fixed-point scheme at device scale (~4 min; QUICK=1 skips the old scheme)

# 4b. refactor safety - resolved WGSL vs git HEAD (edit its tables for a new refactor)
python3 check_refactor_preserving.py
```

All three were last re-run from this location on 2026-09-06 and reproduce the numbers
above exactly (mass 150.00, min z 3.00, mean jp 0.7456). `validate_wgsl.py` and
`check_refactor_preserving.py` were re-run on 2026-09-13 after the dispatcher refactor.

`validate_wgsl.py` is the one worth running habitually — WGSL otherwise only fails at
runtime, and it takes seconds. It keeps a `_resolved_*.wgsl` file for any kernel that fails
so you can look at the flattened source; passing kernels clean up after themselves.

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
MPM_MODEL=drucker_prager ./.venv/bin/python test_mpm.py   # full loop with DP

# 4b. refactor safety - resolved WGSL vs git HEAD (edit its tables for a new refactor)
python3 check_refactor_preserving.py
```

All three were last re-run from this location on 2026-09-06 and reproduce the numbers
above exactly (mass 150.00, min z 3.00, mean jp 0.7456). `validate_wgsl.py` and
`check_refactor_preserving.py` were re-run on 2026-09-13 after the dispatcher refactor.

`validate_wgsl.py` is the one worth running habitually — WGSL otherwise only fails at
runtime, and it takes seconds. It keeps a `_resolved_*.wgsl` file for any kernel that fails
so you can look at the flattened source; passing kernels clean up after themselves.

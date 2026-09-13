# Report: the avalanche that would not cool down

*MLS-MPM snow solver in weBIGeo — investigation and fix of a runaway energy gain, 2026-09-13.*
*Commits `9faafaca` (energy-line instrument) and `4e7ac8d5` (fix).*

## 1. Summary

The simulation gained kinetic energy without bound. A released slab that should slide,
decelerate and stop instead kept accelerating; after two minutes of simulated time the
fastest particle moved at 17 500 m/s. The cause was not in the physics, the constitutive
models or the APIC transfer, but in the **fixed-point integer accumulation** used for the
particle-to-grid scatter: contributions were *truncated* to a 1e-4 quantum, which biases
the node velocity `momentum / mass` upward wherever the stencil weights are small. A thin
slab on a coarse grid makes such weights ubiquitous, and the APIC affine field feeds the
bias back into the particles every step. The fix is two lines of numerics — round instead
of truncate, and give the mass a finer, 64-bit accumulator — plus a regression test that
reproduces the failure offline. After the fix the flow behaves: speeds of 21–26 m/s,
effective friction slightly above the basal coefficient, and the mass comes to rest.

## 2. Symptoms

Observed on the Breite Ries scenario (131 072 particles, `dx = 12.5 m`, `dt = 0.01 s`):

| Quantity | Expected | Observed |
|---|---|---|
| Max particle speed | 30–60 m/s for a real avalanche | 113 m/s at 24 s, 17 526 m/s at 127 s |
| Plastic particle fraction (CCC, sliding slab) | ~10 % (Li et al. 2021, case III) | 100 % |
| Energy-line friction `μ_eff` (basal μ = 0.47) | ≥ 0.47 | 0.31, falling to 0.03 and negative |
| Runout | flow stops in the valley | never stops |

The decisive instrument was the **energy-line readout** added the same day (com1DFA
§5.2): energy height `h_E = z_com + v̄²/2g` must fall by exactly μ per horizontal metre
under Coulomb friction. A `μ_eff` below the basal μ means the solver is *creating* energy,
not merely running fast. That turned "it looks too fast" into a measurable defect.

## 3. Investigation

### 3.1 Facts established on the device

A temporary harness in `AvalanchePanel::draw()` applied a preset, auto-played and logged
`t / vmax / plastic / μ_eff / path / z_com / h_E` after every run. Findings, each from a
separate run:

1. **Model-independent.** Stomakhin, Drucker–Prager and Cohesive Cam Clay all blow up.
2. **Particle-count-independent.** 65 k and 131 k behave the same.
3. **Per-step energy injection is independent of dt.** `dt = 0.005` blew up *sooner* in
   simulated time — so this is not a CFL violation, which would get better with smaller dt.
4. **Purely tangential.** Counters showed no particle ever more than one cell above or
   below the surface; nothing flies, nothing sinks.
5. **The APIC affine matrix `C` is involved.** Dropping `C` from P2G (pure PIC) removes the
   runaway. Halving `C` delays it from 10 s to 55 s. RPIC (rotational part only) and a cap
   `|C·dpos| ≤ |v|` delay it without removing it.

Point 5 is where the investigation went wrong for a while: it made `C` look like the
source. PIC would have "fixed" the symptom at the price of a solver that creeps at 4 m/s
— PIC's numerical dissipation is far too strong for a 400 m runout.

### 3.2 Ruled out

Each of these was implemented, run, and found to change nothing: a particle-level speed
clamp; a strain-rate limit on the deformation-gradient update; a contact band and a
two-sided (both-signs) terrain boundary condition; smoothed terrain normals; sub-cell
slab-friction corrections; the CFL condition.

### 3.3 What found it

Every offline check — the plane drop test, the energy-line test, the material return-mapping
tests — was exact. All of them ran at `dx = 1 m` with a 1.5 m slab: the slab spans more
than one cell and no stencil weight is small.

A vectorised numpy copy of the loop was run at **device scale**: `dx = 12.5 m`, 1.5 m slab
(0.12 cells thick), `dt = 0.01 s`, ~4 000 particles, 35° plane, μ = 0.47. The rigid-block
answer is `a = g(sin θ − μ cos θ) = 1.85 m/s²`. In floating point the mean speed after
20 s was 37.00 m/s against 37.0 analytic — the algorithm is right at that scale too.

Adding a single line — quantise every P2G contribution exactly as the shader did,
`trunc(x · 1e4) / 1e4` — reproduced the blow-up with the same exponential shape as the
device: 161 m/s mean at 20 s, node velocities above 1 000 m/s.

(A first attempt at this reproduction had a bug of its own: `numpy.linalg.svd` returns
`Vᵀ`, and transposing it again turned the rebuilt `F` into a 68° rotation, producing a
convincing but fake "elastic instability". Caught by printing `F` after one step. Worth
remembering: verify the verifier.)

## 4. Root cause

P2G accumulates node mass and momentum with integer atomics because WGSL has no atomic
float add:

```
mass_i     = Σ_p q(w_ip · m_p)
momentum_i = Σ_p q(w_ip · m_p · v_p + …)
v_i        = momentum_i / mass_i
```

with `q(x) = trunc(x · 1e4) / 1e4`. Truncation toward zero takes up to one unit off every
contribution. The *relative* loss depends on the size of the contribution:

- mass contribution with weight `w = 1e-3`: 10 units → up to 10 % lost;
- its momentum contribution at `v = 10 m/s`: 100 units → up to 1 % lost.

The ratio is therefore biased **high**, by roughly `(units lost / units) · (1 − 1/|v|)`,
at every node whose weights are small. This bias is systematic (always the same sign),
acts once per step regardless of `dt`, and is the same for every constitutive model —
exactly the fingerprint from §3.1.

Two things make it fatal in an avalanche setting:

1. **Thin slab, coarse grid.** A 1.5 m slab in a 12.5 m cell is 0.12 cells thick. The
   quadratic B-spline stencil spans three z layers; for a slab this thin the third layer
   receives weights of 1e-3–1e-2 from *every* particle. Those nodes are not a fringe of
   the cloud — they are an entire layer of the flow.
2. **APIC closes the loop.** `C = 4/dx² Σ w vᵢ ⊗ (xᵢ − xₚ)` reconstructs the local
   velocity gradient from the node velocities, weighted by distance. The over-fast nodes
   sit at the far end of the stencil, where `|xᵢ − xₚ|` is largest, so they dominate `C`.
   P2G then scatters `v_p + C·dpos` back to the grid, re-creating the over-fast layer, and
   the cycle repeats. PIC, which discards `C`, only inherits the bias through the tiny
   direct weights and shows a mild drift — which is why removing `C` looked like a cure.

## 5. Fix

`mpm_common.wgsl`, `mpm_p2g.wgsl`, `mpm_clear_grid.wgsl`, `mpm_grid_update.wgsl`,
`GRID_NODE_STRIDE_U32` 4 → 5.

1. **Round, never truncate.** `to_fixed` is now `i32(round(x · 1e4))`. Rounding errors are
   zero-mean, so the sums are unbiased.
2. **Mass gets its own scale and a 64-bit accumulator.** `add_node_mass()` accumulates
   `round(w · 2²⁰)` into a `u32` lo/hi pair; the carry is detected from the value
   `atomicAdd` returns (`old > 0xFFFFFFFF − x` ⟹ it wrapped), the same trick already used
   for the `SimState` sums. Precision 1e-6, range unlimited, one extra atomic only on a
   carry. This removes the residual ratio noise at nodes whose mass is only a few units.
3. **Momentum stays `i32` at 1e-4.** It is signed, and a signed 64-bit emulation would
   double the atomics in the hot loop. Its range is now stated explicitly: particles per
   node × speed must stay below 2·10⁵ momentum units. At 450 particles per cell (131 k in
   a 120 m disc at `dx = 12.5`) that allows ~450 m/s, far beyond anything physical; only a
   very dense seeding can reach it. Documented in `05-tuning.md`.

Both halves are needed. Emulated offline against the rigid block at 20 s:

| Scheme | Mean speed | Error |
|---|---|---|
| floating point | 37.00 m/s | 0.0 % |
| old: truncate, both at 1e-4 | 161 m/s | +336 % (runaway) |
| round only, both at 1e-4 | 37.89 m/s | +2.4 % and still drifting |
| truncate, mass at 1e-6 | 34.18 m/s | −7.6 % (momentum truncation is dissipative) |
| **new: round, momentum 1e-4, mass 2⁻²⁰** | **36.97 m/s** | **−0.1 %** |

## 6. Verification

**Offline.** `mpm-mls-doc/scripts/test_sheet_fixed_point.py` is the device-scale slab
test with both quantisation schemes emulated; it passes when floats and the shader scheme
are within 3 % of the rigid block and prints the old scheme for comparison. The existing
suite (`test_mpm.py` × 3 models, `test_energy_line.py`, `test_svd.py`, the material
tests) still passes; the energy-line test recovers the basal μ to 0.3024 vs 0.3.

**On device**, same harness, same scenario, 144 s simulated:

| | Before | After |
|---|---|---|
| Stomakhin: max speed | 113 → 17 000+ m/s | 21–26 m/s throughout |
| Stomakhin: `μ_eff` (basal 0.47) | 0.03 … negative | 0.58 → 0.49 |
| Stomakhin: runout | never stops | stops after 380 m |
| CCC sliding slab: max speed | runaway | ≤ 25 m/s |
| CCC sliding slab: plastic fraction | 100 % | 25 % → 55 % |
| CCC sliding slab: `μ_eff` | 0.31 falling | 0.60 → 0.49 |
| CCC sliding slab: runout | never stops | stops after 340 m |

`μ_eff` sitting ~0.02 above the basal μ once the flow is established is the internal
(plastic) dissipation on real terrain — small, positive, and stable. The energy line now
decreases monotonically.

**Residual.** About 90 of 131 072 particles keep moving at more than 10 m/s after the
mass has stopped. These are lone particles on a coarse grid: their stencils barely touch
the terrain layer, so they see almost no basal friction. Cosmetic at < 0.1 %; noted in
`05-tuning.md`, could be hidden with a density threshold in the overlay.

## 7. Lessons

- **Test the numerics at production scale, including the integer arithmetic.** The
  floating-point port of the algorithm passed every test and was correct. The bug lived
  entirely in the quantisation, which no offline test emulated, at a grid-to-slab ratio
  no offline test used.
- **A quantity that must be conserved is the best instrument.** The energy line turned a
  vague "too fast" into "energy is being created", ruled out whole classes of explanation
  (a CFL problem would scale with dt; a boundary problem would show particles leaving the
  surface) and gave a pass/fail criterion for the fix.
- **Amplifier ≠ source.** Removing `C` removed the symptom; that was evidence against `C`
  only in the sense that a fire goes out without oxygen. Ask what the amplified signal is
  before removing the amplifier.
- **Truncation toward zero is not a harmless rounding mode** when the result is a ratio
  of two sums with different magnitudes. Rounding costs nothing and would have prevented
  the whole episode.

## 8. Files

| File | Change |
|---|---|
| `webgpu/compute/shaders/mpm_common.wgsl` | `MOMENTUM_SCALE` / `MASS_SCALE`, `GridNode` with `mass_lo/hi`, `to_fixed` rounds, `add_node_mass()`, `node_mass()` |
| `webgpu/compute/shaders/mpm_p2g.wgsl` | uses `add_node_mass()` |
| `webgpu/compute/shaders/mpm_clear_grid.wgsl`, `mpm_grid_update.wgsl` | new mass fields |
| `webgpu/compute/nodes/MpmSolverNode.cpp` | `GRID_NODE_STRIDE_U32 = 5` |
| `mpm-mls-doc/scripts/test_sheet_fixed_point.py` | new regression test |
| `mpm-mls-doc/03…07` | pseudo-code, layout, failure modes, §4h, bug 8, step 6 addendum |
| `claude_log.md` | entry 8 for 2026-09-13 |

# Constitutive & friction models — status and plan

Written 2026-09-13 against proposal **v3.0** and the *Snow/Avalanche Sim Paper Summaries*
Notion page. Answers two questions: which of the models named there are actually in the code
today, and how to make the model layer configurable and exchangeable.

The framing from the paper summaries is the right one and worth keeping in your head:
**the flow solver and the material model are independent layers.** MLS-MPM is the solver.
The constitutive model is what makes it snow. Basal friction is a *third* thing — a boundary
condition, not part of either. v3.0 already keeps these verbally separate; the code should
too.

## 1. What proposal v3.0 asks for (delta vs v1.0)

| v3.0 goal | Status |
|---|---|
| MLS-MPM solver | **done** |
| A constitutive model — Stomakhin [5] *or* successors Gaume CCC [11], Li et al. [12,13] | **done** — Stomakhin, Drucker–Prager and Cohesive Cam Clay behind a switch |
| Model is configurable / regime presets (Li 2021 Table 1) | switch done; **presets still to do** (step 5) |
| Basal friction via simple Coulomb using terrain normals | **done**; Voellmy added alongside (2026-09-13) |
| Single-particle rendering | not done (2D density overlay instead) |
| Entrainable material along the flow path | **not done** |
| Compare against non-real-time models (com1DFA / Flow-Py) | not done |
| CK-MPM (stretch) | not done |

## 2. What is implemented — verified against the shader source

### 2a. Stomakhin et al. 2013 — fully implemented

`mpm_material_stomakhin.wgsl`, `stomakhin_stress()` + `stomakhin_plasticity()` (was
`snow_stress()` / `apply_plasticity()` in `mpm_common.wgsl` before the 2026-09-13 refactor).
Checked line by line:

| Component | In code | Notes |
|---|---|---|
| Multiplicative split F = Fᴱ Fᴾ | yes | Fᴾ tracked implicitly via `plastic_state` (= Jp) |
| Fixed-corotated elasticity `2μ(F−R)Fᵀ + λJ(J−1)I` | yes | R = UVᵀ from the SVD |
| Singular-value box clamp `[1−θc, 1+θs]` | yes | θc = 2.5e-2, θs = 7.5e-3 |
| Exponential hardening `h = exp(ξ(1−Jp))` | yes | ξ = 10, h clamped to [0.05, 20] |
| Jp update `Jp ← Jp · Π(σᵢ/σ̂ᵢ)` | yes | clamped to [0.05, 20] |
| Paper Table 1 defaults | yes | E = 1.4e5, ν = 0.2, ρ = 400 |

Verified offline: plastic compaction on impact (`jp` 1.0 → 0.746), see
[06-verification.md](06-verification.md). This is a complete, correct Stomakhin.

### 2b. Coulomb basal friction — implemented, matches v3.0

`resolve_terrain_collision()`, applied at **both** the grid level (`mpm_grid_update`, as a
boundary condition on nodes below the surface) and the particle level (`mpm_g2p`, position
clamp + same response). Uses terrain normals from central differences of the DEM:

```
vn = v·n
if vn ≥ 0:            separating, untouched
vt = v − n·vn
if |vt| ≤ −μ·vn:      v = 0                      # sticking
else:                 v = vt·(1 + μ·vn/|vt|)     # sliding
```

This is exactly the "simple Coulomb friction model that uses terrain normals" v3.0 asks for,
applied where the paper summaries recommend (grid-update pass).

**Done 2026-09-13:** μ default 0.4 → **0.47** (Li et al. 2021, real terrain). **Voellmy** added
as a second `BasalFrictionModel` — Coulomb plus the turbulent term `g|v|²/(ξ·h)`, ξ = 4000
default, `h` = `slab_thickness` as the reference depth so ξ stays in com1DFA units (the
trajectories node uses the same convention with `h` hard-coded to 1 m). The drag is applied
at the **grid level only** via an `apply_basal_drag` flag — it depends on `|v|²` not `vn`, so
applying it at the particle level too would double-count it per substep. Coulomb ignores
the flag and is bit-identical to before. Verified against the analytic terminal velocity
`v∞ = √(ξh(sinθ − μcosθ))` to 0.085% — see [06-verification.md](06-verification.md).

### 2d. Drucker–Prager (Klár 2016) — implemented 2026-09-13

`mpm_material_drucker_prager.wgsl`, `ConstitutiveModel::DRUCKER_PRAGER = 1`. Hencky
(log-strain) elasticity `τ = 2με + λ tr(ε) I` in the principal frame, `P Fᵀ = U diag(τ) Uᵀ`;
yield surface the friction cone `‖dev τ‖ + α tr τ ≤ 0` with
`α = √(2/3)·2 sinφ/(3 − sinφ)` precomputed CPU-side from `dp_friction_angle` (default 30°,
Klár's sand). Return mapping is Klár §5.3's closed-form projection in Hencky-strain space:

```
ε = log Σ,   ε̂ = ε − tr(ε)/3
if tr(ε) > 0:                          Case II  – tension, project to the apex (ε ← 0)
δγ = ‖ε̂‖ + (3λ+2μ)/(2μ) · tr(ε) · α
if δγ ≤ 0:                             Case I   – elastic
else:  ε ← ε − δγ · ε̂/‖ε̂‖             Case III – onto the cone
```

`plastic_state` accumulates δγ (a free "has this particle yielded" diagnostic); Klár's
hardening of φ is left out. Cohesionless, so cold-dense regime only — the documented
fallback if CCC is too slow. Verified: `test_material_dp.py` checks every Case III output
lands on the cone to `|y| < 1.5e-10` over 500 random gradients; `test_mpm.py` with
`MPM_MODEL=drucker_prager` shows the qualitative signature (spreads instead of piling).

**One bug caught by the test, worth remembering:** the first draft sent *pure hydrostatic
compression* to the cone apex because it guarded Case II with `‖ε̂‖ = 0 OR tr > 0`.
Hydrostatic compression has `ε̂ = 0` but sits *inside* the cone on its axis and must stay
elastic. With `tr ≤ 0` the second term of δγ is ≤ 0, so `δγ > 0` already implies
`‖ε̂‖ > 0` — the division guard was unnecessary and wrong. Condition is `tr > 0` alone.

**Design change from §3e:** E and ν are **shared** across models rather than duplicated as
`hencky_*`. Two stiffness knobs that silently disagree is worse than one; the CFL readout
already keys off the shared E; per-model recommended values are what presets (step 5) are
for. DP therefore needed one new field (`dp_alpha`, in the last pad slot) and the uniform
stayed 160 B. Enum numbering is also `DRUCKER_PRAGER = 1` (not 2) so the combo stays
contiguous until CCC lands.

### 2e. Cohesive Cam Clay (Gaume 2018) — implemented 2026-09-13

`mpm_material_ccc.wgsl`, `ConstitutiveModel::COHESIVE_CAM_CLAY = 2`. Same Hencky elasticity
as DP; the yield surface is Gaume's ellipse in (p, q):

```
p = −K tr ε,   q = √(3/2)·2μ‖dev ε‖,   K = λ + 2μ/3
y = (1+2β)q² + M²(p + βp₀)(p − p₀) ≤ 0        admissible p ∈ [−βp₀, p₀]
p₀ = K·sinh(ξ·max(−α, 0))                     α = plastic volumetric strain
```

`plastic_state` is α, initialised to `−asinh(p₀ⁱⁿⁱ/K)/ξ` so that `p₀(α₀) = p₀ⁱⁿⁱ` exactly
(`ccc_initial_state()` — this is why the initial state had to be a dispatcher call).

**Return mapping — a documented choice.** Gaume 2018 describes an associative flow rule,
which needs a per-particle Newton solve on the ellipse. Implemented instead is the
three-case projection of **Wolper et al. 2019 (NACC)** — the same group's implementation
of the same surface and hardening law:

```
Case 1  p > p₀          → (p₀, 0),      α += tr ε + p₀/K      (compaction: harden)
Case 2  p < −βp₀        → (−βp₀, 0),    α += tr ε − βp₀/K     (dilation: soften)
Case 3  y > 0 otherwise → q onto the ellipse at fixed p, α unchanged
```

Explicit: the old p₀ is used for the projection, then hardening is updated. Case 3's
`q_new/q` division is safe by construction: with p in range the ellipse term is ≤ 0, so
`y > 0` implies `q > 0`. The sinh argument is clamped at 20 so runaway compaction cannot
overflow. Swapping in an associative return only touches `ccc_plasticity()`.

Defaults are Li 2021 **Case V** — the case back-calculated from the real Vallée de la
Sionne avalanche (M 0.7, β 0.2, ξ 0.002, p₀ 3 kPa) — as the most defensible single
setting; Cases I–IV become presets. Four new uniform fields; **uniform now 176 B**, no
pad slots left.

**Verified** (`test_material_ccc.py`, 12 checks): initial α round-trips to p₀ exactly;
Case 3 lands on the ellipse (`|y| ≈ 7e-8` on a 1e7-scale surface) *at the same p*; Case 1
returns to `(p₀, 0)` and hardens; Case 2 returns to `(−βp₀, 0)` and softens; 500 random
gradients never leave the surface; repeated tension softens monotonically to `p₀ = 0`
(fracture); repeated compression hardens until admissible then stops; β = 0 has no
tensile strength; all five Li 2021 Table 1 cases produce finite initial states that
round-trip at E = 3 MPa.

In the drop test the **parameter → behaviour link works**: weak Case V (p₀ 3 kPa)
flattens to a pancake (mean z 3.02) because impact pressure ~14 kPa is far above the cap
and Cam-Clay loses shear strength above p₀ — the opposite of DP's cone; strong Case III
(p₀ 42 kPa) piles at 4.13, alongside Stomakhin's 4.00. Both come to rest, unlike DP.

### 2c. Not implemented

- ~~**Gaume 2018 Cohesive Cam Clay**~~ — **done 2026-09-13**, see §2e.
- **Li 2020/2021 regime presets** — the parameters now exist (M, β, ξ, p₀); the preset
  table itself is step 5.
- ~~**Model selection**~~ — **done 2026-09-13.** `ConstitutiveModel` / `BasalFrictionModel`
  enums, `u32` uniform fields, runtime `switch` dispatchers, combos in both panels.
- ~~**Drucker–Prager**~~ — **done 2026-09-13**, see §2d.
- **Entrainment**, **energy-line validation**, **comparison to com1DFA/Flow-Py** — nothing.

## 3. Making models exchangeable — design

### 3a. Follow the codebase precedent: runtime switch on a uniform enum

`ComputeAvalancheTrajectoriesNode` already does this: `PhysicsModelType` / `FrictionModelType`
C++ enums → `model_type: u32` / `runout_model_type: u32` in the uniform → `if`/`switch` in
WGSL → a `Combo` in the node renderer. Same pattern here.

Why runtime rather than compile-time (`///if` variants):
- The shader preprocessor's defines are **global**, not per-registration, so per-model
  compiled variants would need `compile_shader_from_code()` plumbing and pipeline recreation
  on every switch. The precedent avoids all of that.
- The branch is on a **uniform**, so every particle takes the same path — no divergence,
  negligible cost. The only cost is a larger shader.
- Switching from the UI is instant and needs no pipeline rebuild.

### 3b. Two independent enums

```cpp
enum ConstitutiveModel  : uint32_t { STOMAKHIN_2013 = 0, DRUCKER_PRAGER = 1, COHESIVE_CAM_CLAY = 2 };
enum BasalFrictionModel : uint32_t { COULOMB = 0, VOELLMY = 1 };
```

(As implemented. DP took 1 because it landed first; numbering is arbitrary, contiguity is
what the combos need.)

Kept separate on purpose — v3.0 and the paper summaries both stress that internal friction
(M, part of the constitutive model) and basal friction (μ, a boundary condition) must not be
conflated. Two enums makes that structural.

### 3c. The interface a constitutive model must satisfy

Only two touch points in the whole loop:

```
stress(F, plastic_state)          → P·Fᵀ            used in mpm_p2g
plasticity(F_trial, plastic_state) → (F, plastic_state)   used in mpm_g2p
```

Per-particle state needed by each model is **one scalar**:

| Model | `plastic_state` means | Elasticity |
|---|---|---|
| Stomakhin | Jp, plastic volume ratio | fixed corotated |
| CCC | α, plastic volumetric strain (drives p₀ via sinh); starts at −asinh(p₀ⁱⁿⁱ/K)/ξ | Hencky |
| Drucker–Prager | accumulated plastic strain Σδγ (diagnostic only) | Hencky |

So the `Particle` struct **does not change** — rename `jp` → `plastic_state` and document its
meaning per model. Elasticity is part of the model, so it lives inside each model's
`stress()`; no shared "elastic law" abstraction needed.

### 3d. File layout

Split the constitutive section out of `mpm_common.wgsl`:

```
mpm_material_stomakhin.wgsl       stomakhin_stress(), stomakhin_plasticity()
mpm_material_ccc.wgsl             ccc_stress(), ccc_plasticity()
mpm_material_drucker_prager.wgsl  dp_stress(), dp_plasticity()
mpm_material.wgsl                 material_stress(), material_plasticity()  — the dispatcher
```

Each model file is self-contained and `///use`d by the dispatcher. `mpm_p2g` / `mpm_g2p` call
only the dispatcher. **Adding a model = one new file + one `case` + one enum entry + one
combo item.** Same for `mpm_friction.wgsl` dispatching Coulomb / Voellmy.

### 3e. Uniform and settings

Uniform gains `constitutive_model: u32`, `basal_friction_model: u32`, and the **union** of
all model parameters — unused ones are simply ignored by the active branch:

```
// CCC (Gaume 2018 / Li 2021)
ccc_friction_m, ccc_cohesion_beta, ccc_hardening_xi, ccc_p0_initial
// Drucker–Prager
dp_friction_angle
// Voellmy
voellmy_xi
// Hencky elasticity (CCC, DP) — separate E/ν because Li uses 3 MPa / 0.3, not Stomakhin's
hencky_youngs_modulus, hencky_poissons_ratio
```

That is ~10 floats → uniform grows by 48 B to 192 B. Follow the add-a-field recipe in
[05-tuning.md](05-tuning.md) — both sides, offsets by hand.

Settings struct mirrors it. Serialisation: model enums as ints, same as `PhysicsModelType`.

### 3f. Presets are a UI concept on top of the parameters

A `MaterialPreset { name, model, params }` table in `AvalanchePanel`, applied via a combo.
Presets **write** parameters; they are not a third configuration path. Entries from Li 2021
Table 1 (the regime cases were designed for the same slope):

| Preset | Model | M | β | ξ | p₀ (kPa) | ρ | E | ν | μ |
|---|---|---|---|---|---|---|---|---|---|
| Cold dense | CCC | 0.5 | 0 | 1 | 3 | 250 | 3 MPa | 0.3 | 0.47 |
| Warm shear | CCC | 1.5 | 0.3 | 1 | 30 | 250 | 3 MPa | 0.3 | 0.47 |
| Sliding slab | CCC | 1.5 | 0.5 | 1 | 42 | 250 | 3 MPa | 0.3 | 0.47 |
| Warm plug | CCC | 0.5 | 1.0 | 0.1 | 12 | 250 | 3 MPa | 0.3 | 0.47 |
| VdlS 2003 (verification) | CCC | 0.7 | 0.2 | 0.002 | 3 | 200 | 3 MPa | 0.3 | 0.49 |
| Stomakhin 2013 | Stomakhin | — | — | 10 | — | 400 | 1.4e5 | 0.2 | 0.47 |

Two traps from the paper summaries, now baked into the table so they aren't rediscovered:
**warm plug has the lowest M with the highest β** (counter-intuitive but correct), and **ξ is
what separates sliding slab from warm plug**, not Mβ. Also: Cases II and III did *not*
reproduce their 2D regimes on real terrain in Li 2021 — expect re-tuning, say so in the
report.

The existing `Scenario` struct (location) and a material preset are orthogonal; a scenario
can name a default preset but the two stay separate.

## 4. Next steps, in order

Ordered so each step is independently useful and the risky physics comes after the plumbing
that will be needed to test it.

**1. Refactor for exchangeability — no new physics.** ✅ **Done 2026-09-13.** Split
`mpm_material_stomakhin.wgsl` out, added `mpm_material.wgsl` / `mpm_friction.wgsl`
dispatchers, both enums, the uniform/settings fields (144 → 160 B), the two combos, renamed
`jp` → `plastic_state`, added `material_initial_state()` to the interface (the initial
plastic state is model-dependent: Jp = 1 vs εᵥᵖ = 0). Proven behaviour-preserving by diffing
the resolved WGSL against HEAD — see [06-verification.md](06-verification.md#4b-refactor-safety--resolved-shader-diff-against-head).
All 8 pipelines confirmed on Metal.

**2. Basal friction: μ → 0.47, add Voellmy.** ✅ **Done 2026-09-13.** See §2b. First real
second case in a dispatcher; `test_friction.py` checks it.

**3. Drucker–Prager (Klár 2016).** ✅ **Done 2026-09-13.** See §2d. Hencky elasticity is
now in and verified, which CCC reuses.

**4. Cohesive Cam Clay (Gaume 2018).** ✅ **Done 2026-09-13.** See §2e — including the
return-mapping choice (Wolper's three-case projection rather than the associative rule).

Still true and still worth doing: **benchmark it.** Nobody has run CCC interactively. Per
particle it is one SVD plus a handful of scalars — same order as Stomakhin — so it should be
fine, but that is an expectation, not a measurement. And the CFL note stands: at Li's
E = 3 MPa, ρ = 250 the wave speed is ~110 m/s vs ~19 m/s at Stomakhin's values, so the
allowed dt drops ~6×. The CFL readout already uses the shared E, so switching a preset will
show it.

**5. Regime presets** from §3f in the sidebar, plus a plastic-particle-ratio readout
(Li 2021 reports 77 / 26 / 10 / 34 % for Cases I–IV — a cheap sanity metric that needs the
`active_particles`-style readback that is still missing).

**6. Energy-line validation** (com1DFA §5.2). GPU reduction of centre-of-mass and kinetic
energy per run, then check the energy balance along the path. Answers "how do you know this
isn't nonsense" without needing real-avalanche data, which v3.0 puts out of scope.

**7. Entrainment.** Input: an entrainable-snow-depth texture (v3.0 says computing it is out
of scope). Cheapest MPM-compatible mechanism: a flowing particle over a cell with entrainable
depth **gains mass and volume** and the cell's depth decrements — no particle spawning, no
free list. Note this breaks the current "mass = 1" invariant into "mass = 1 base unit,
growing"; the fixed-point range still holds. Rate law needs research (Sovilla-style
erosion ∝ velocity is the usual starting point). The CCC papers explicitly exclude
entrainment and blame their velocity discrepancies on it — state that.

**8. Comparison to com1DFA / Flow-Py** (v3.0's final stage) — needs 6 first.

## 5. Sources

Numbering per the proposal. Full entries in [refs.md](refs.md).

- [5] Stomakhin 2013 — implemented model
- [11] Gaume, Gast, Teran, van Herwijnen, Jiang 2018 — CCC, *Nat. Commun.* 9, 3047
- [12] Li, Sovilla, Jiang, Gaume 2020 — regimes, *The Cryosphere* 14, 3381
- [13] Li, Sovilla, Jiang, Gaume 2021 — 3D real terrain + Table 1, *Landslides* 18, 3393
- Klár et al. 2016 — Drucker–Prager, *ACM TOG* 35(4)
- Tonnel et al. 2023 — com1DFA (Coulomb/Voellmy/samosAT, energy line test), *GMD* 16, 7013
- Full paper notes: Notion → *Snow/Avalanche Sim Paper Summaries*

# Implementation audit — MLS-MPM avalanche solver vs. papers (independent report)

*Issue [#3](https://github.com/Emanum/webigeo/issues/3). Written by Claude Opus 5.5 on 2026-10-03, on
`main` at `a7626e8e`, **before** reading `report-implementation-audit.md` (the other model's report).*

## Scope and method

The issue asks for two things: check the docs in `mpm-mls-doc/`, and compare the implementation with
the papers, in particular the formulas. This report covers the solver path end to end:

- the transfers (`mpm_p2g`, `mpm_grid_update`, `mpm_g2p`),
- the three constitutive models (`mpm_material_*.wgsl`),
- basal friction (`mpm_friction.wgsl`),
- the parameter derivations and presets (`MpmSolverNode::update_gpu_settings`, `AvalanchePanel` presets),
- the diagnostics (energy line, plastic ratio).

**Sources read for this audit.** Text was extracted from the PDFs in the user's paper folder:

| Paper | In folder | Used for |
|---|---|---|
| Hu et al. 2018, MLS-MPM | yes | transfers, force term (Eq. 15–18), algorithm (§4) |
| Stomakhin et al. 2013 | yes | energy density (Eq. 1–2), Table 2, plasticity (§7), collisions (§8) |
| Gaume et al. 2018 | yes | CCC yield surface (Eq. 3), hardening (Eq. 4), Methods (elasticity, flow rule, return mapping) |
| Li et al. 2020 (The Cryosphere) | yes | model summary, numerics (Table 1) |
| Li et al. 2021 (Landslides) | yes | Table 1 (presets), numerics, plastic-particle ratio |
| Tonnel et al. 2023 (com1DFA) | yes | friction laws (§2.3), energy-line test (§5.2, App. A) |
| Klár et al. 2016 (Drucker–Prager) | **no** | checked against the published closed form from memory; marked as such |
| Wolper et al. 2019 (CD-MPM) | **no** | not verifiable here; marked as such |

Severity: **High** means it changes what the simulation shows or invalidates a claim. **Medium** means a
real deviation with a limited or situational effect. **Low** means a minor deviation or guard.
**Info** means no action needed beyond documentation.

## Summary

The **formulas that are implemented are almost all implemented correctly.** That covers:

- the MLS-MPM force and affine terms,
- Stomakhin's fixed-corotated stress, hardening and singular-value plasticity,
- the Drucker–Prager cone and its closed-form return,
- the Cohesive Cam Clay yield function, p/q invariants, hardening law and initial state,
- every Li et al. 2021 preset value.

The important findings are about *what is implemented* and *what the outputs are compared with*:

1. **High: the Cam Clay flow rule is not the papers'.** Gaume 2018 and both Li papers use an
   **associative** flow rule. The code uses a non-associative fixed-p projection, so shear yielding
   never changes volume and never hardens or softens the material. The four Li "flow-regime" presets
   were calibrated with the associative rule, so they cannot be expected to reproduce Li's regimes.
2. **High: resolution is 25× coarser than Li 2021.** The grid is 12.5 m against 0.5 m. The release
   slab is 0.12 cells thick against 2 cells, there are ~300 particles per cell against 8, and the
   hardening factor isn't rescaled for the mesh (Li suggests adjusting ξ for mesh size). Together with
   (1), this makes the regime presets labels rather than reproductions.
3. **Medium: the plastic-particle ratio isn't comparable to Li 2021.** The code counts particles that
   have *ever* yielded and compares that with Li's 77 / 26 / 10 / 34 %. Li's numbers are the share of
   particles *at* the yield surface at t = 10 s.
4. **Medium: the energy line uses the centre of mass's own path.** Tonnel's test (App. A, Eq. A4–A5)
   uses the mass-averaged path length of the material points. For a spreading flow the centre-of-mass
   path is shorter, so `μ_eff` is biased high.
5. **Medium: the Voellmy term uses a constant depth and is attributed to the wrong source.** It uses
   the constant `slab_thickness` instead of the local flow depth. The docs call μ 0.155 / ξ 4000
   "com1DFA's default", but com1DFA's default friction model is samosAT; those values are only its
   Voellmy defaults, and Tonnel 2023 names Voellmy without giving the formula.

Each of these is detailed below, along with smaller items and the list of what was verified correct.

---

## 1. MLS-MPM transfers (Hu et al. 2018)

**Verified correct**
- **Kernel:** quadratic B-spline weights and base node (`compute_kernel`). These are the standard
  forms, with `base = floor(x/dx − 0.5)`.
- **P2G** (`mpm_p2g.wgsl`): momentum `w·(m v + A (x_i − x_p))` with
  `A = −Δt V⁰ (4/Δx²) τ + m C`. This is Hu Eq. 18 with M⁻¹ = 4/Δx² for quadratic B-splines (Eq. 15), and
  `τ = (∂Ψ/∂F) Fᵀ` of the elastic state at tⁿ. `V⁰` is the initial particle volume, as in Eq. 18.
- **G2P:** `C = (4/Δx²) Σ w v_i (x_i − x_p)ᵀ`. The code writes this as `4/dx · w · v ⊗ dpos` with
  `dpos` in grid units, which is the same thing. `F ← (I + Δt C) F` is Eq. 17, followed by plasticity
  and then advection. This is exactly the step order of Hu §4.
- **Stress at tⁿ:** P2G uses the τ that the previous G2P computed from its own return mapping. That is
  the stress of F_Eⁿ, as Eq. 18 requires. After a parameter edit, `mpm_refresh_stress` recomputes it.
- **Mass normalisation** (m = 1, V⁰ = 1/ρ): scaling mass and volume by the same factor leaves the
  discrete equations invariant. That's correct and documented.

**Deviations (Info)**
- **Explicit integrator and APIC.** The scheme is explicit symplectic Euler with APIC, which is Hu's
  choice and Li 2021's. Stomakhin 2013 used semi-implicit integration and a PIC/FLIP blend (α = 0.95).
  See §2 for why that matters for Stomakhin's parameters.
- **Fixed-point grid accumulation.** P2G accumulates in fixed point: momentum at 1e-4, rounded, and
  mass at 2⁻²⁰ in 64 bits. That's an implementation necessity in WGSL with no counterpart in the
  papers. It is verified offline (`test_sheet_fixed_point.py`).

## 2. Stomakhin et al. 2013

**Verified correct**
- **Energy and stress.** The energy is Ψ = μ(J_P)‖F_E − R_E‖² + λ(J_P)/2 (J_E − 1)² (Eq. 1). The
  Kirchhoff stress τ = 2μ(F_E − R_E)F_Eᵀ + λ J_E(J_E − 1) I is evaluated in the SVD frame as
  `2μ(σ−1)σ + λJ(J−1)`, which is correct.
- **Hardening.** μ = μ₀ e^{ξ(1−J_P)} and λ = λ₀ e^{ξ(1−J_P)} (Eq. 2).
- **Plasticity.** The trial F_E's singular values are clamped to [1−θc, 1+θs] (§7), and J_P is
  updated so that the total J is conserved (`jp · Π σ/σ_clamped`). This is equivalent to
  F_P = V Σ⁻¹ Uᵀ F.
- **Defaults** match Table 2 (θc 2.5e-2, θs 7.5e-3, ξ 10, ρ 400, E 1.4e5, ν 0.2).

**Deviations**
- **Low: clamps not in the paper.** The hardening factor is clamped to [0.05, 20] and J_P to
  [0.05, 20]. Packed snow therefore stops stiffening at 20× instead of e^{9.5} ≈ 13 000×. This is a
  sensible stability guard, but it changes compaction behaviour and should be documented as a
  deviation.
- **Info: parameters tuned for another integrator.** Table 2 was tuned for a semi-implicit integrator
  and FLIP, which add damping. The explicit APIC solver here is less dissipative, so the same
  parameters can look more energetic. That fits the "film snow" label of the preset.

## 3. Drucker–Prager (Klár et al. 2016)

*The paper isn't in the folder; checked against the published closed form.*

**Consistent**
- **Elasticity:** Hencky strain ε = log Σ, with τ = 2μ ε + λ tr(ε) I.
- **Cone slope:** α = √(2/3) · 2 sin φ / (3 − sin φ).
- **Plastic multiplier:** δγ = ‖ε̂‖ + (dλ + 2μ)/(2μ) · tr(ε) · α with d = 3.
- **Cases:** Case II (tr ε > 0) projects to the tip with δq = ‖ε‖, Case I (δγ ≤ 0) stays elastic, and
  Case III projects as ε − δγ ε̂/‖ε̂‖.
- **Preset angle:** the preset's 13.3° comes from sin φ = 3M/(6+M) with M = 0.5. That matches the
  Mohr–Coulomb compression-cone M = 6 sin φ / (3 − sin φ).

**Deviations**
- **Low: no friction-angle hardening.** Klár's hardening of the friction angle over the accumulated
  plastic strain is left out. This is documented.
- **Low: a contradicted claim.** The shader header and the docs say Drucker–Prager is "cheaper than
  Cam Clay … the fallback if CCC proves too slow". The M5 benchmark contradicts that: Drucker–Prager
  costs 0.66 ms per substep against Cam Clay's 0.64 ms. Both are dominated by the same SVD.

## 4. Cohesive Cam Clay (Gaume et al. 2018, Li et al. 2020/2021)

**Verified correct**
- **Pressure.** p = −tr(τ)/3 = −K tr(ε) with K = λ + 2μ/3.
- **Equivalent stress.** q = √(3/2)‖s‖ = √(3/2) · 2μ ‖dev ε‖, as in Gaume Eq. 1–2.
- **Yield function.** y = (1+2β)q² + M²(p+βp₀)(p−p₀) (Gaume Eq. 3, Li 2020 Eq. 4).
- **Hardening.** p₀ = K sinh(ξ max(−ε_V^P, 0)) (Gaume Eq. 4). Here α accumulates
  `trace_trial − trace_returned` = log(J_trial / J_returned), which is the change of log det F_P that
  Gaume's ε_V^P is defined with.
- **Initial state.** α is initialised so that p₀(α) = p₀ⁱⁿⁱ, matching Li's "initial consolidation
  pressure".
- **Cap and tip.** The cap (p > p₀ → (p₀, 0)) and the tip (p < −βp₀ → (−βp₀, 0)) return to the right
  points with the right sign of α (compaction hardens, dilation softens).
- **Shear case at fixed p.** For p inside the range, the q on the ellipse,
  M·√((p+βp₀)(p₀−p)/(1+2β)), solves y = 0 correctly.

**High: wrong flow rule for the cited papers.** Gaume 2018 (Methods, Eq. 12–13), Li 2020 ("this study
uses an associative plastic flow rule reported by Simo (1992) and Simo and Meschke (1993)") and
Li 2021 ("… a hardening law, and an associative flow rule") all use **associative** plasticity. That
return projects the trial stress onto the ellipse in the C⁻¹ (energy) norm. The code instead keeps p
fixed and scales q down; the docs call it Wolper's non-associative "NACC" return.

Consequences:
- **No hardening or softening in shear.** Under the associative rule, shear yielding also produces
  volumetric plastic strain: dilation, which softens, on the tensile side of the ellipse's crown, and
  compaction, which hardens, on the compressive side. With the fixed-p return, shear changes neither α
  nor p₀. Hardening and softening happen only when the trial state passes the cap or the tip.
- **The presets are calibrated for the other rule.** Li's regime presets (Table 1) were calibrated
  with the associative rule. Brittle "sliding slab" break-up and "warm shear" granulation rely on
  shear-induced softening, which the fixed-p return removes.

The docs state the choice (01-theory, 07 §2e, refs), so it isn't hidden. But the presets carry Li's
regime names, and the panel's notes ("breaks into blocks shortly after release", …) describe Li's
results, not this model's. *Wolper 2019 isn't in the folder, so I couldn't check that the fixed-p
projection is exactly Wolper's.* The recommendation is to implement the associative return:
Gaume Supplementary Note 1, Simo–Meschke. Keep the fixed-p return as an option, and compare.

**High: resolution and regularisation.** Li 2021 runs at Δx = 0.5 m with 8 particles per cell,
Δt = 2·10⁻³ s, 1.9 M particles and a 1.05 m release depth. Li 2020 uses Δx = 0.05 m. The app runs at
Δx = 12.5 m, a 1.5 m slab (0.12 cells) and ~300+ particles per cell. Li 2021 also notes that ξ can be
adjusted per mesh resolution so the fracture energy stays the same, and the code doesn't do that.

The regime behaviour lives at the scale of the slab, below one cell here. So even with the right flow
rule, the presets would not produce Li's regimes at this resolution. The docs already call resolution
"the honest limit" (05-tuning, 08). This report adds that **the preset labels promise what the model
can't deliver at this scale.**

**Low: guards.** These clamps aren't in the papers:
- the sinh argument is clamped to 20,
- σ is clamped to [10⁻³, 10³] before the log,
- M ≥ 1e-3 and ξ ≥ 1e-6.

They're fine; they just aren't documented as deviations.

**Info: presets checked against Li 2021 Table 1.** All five Cam Clay presets match exactly:

| Preset | M | β | ξ | p₀ⁱⁿⁱ | ρ | E | ν | μ_basal |
|---|---|---|---|---|---|---|---|---|
| I cold dense | 0.5 | 0 | 1 | 3 kPa | 250 | 3 MPa | 0.3 | 0.47 |
| II warm shear | 1.5 | 0.3 | 1 | 30 kPa | 250 | 3 MPa | 0.3 | 0.47 |
| III sliding slab | 1.5 | 0.5 | 1 | 42 kPa | 250 | 3 MPa | 0.3 | 0.47 |
| IV warm plug | 0.5 | 1.0 | 0.1 | 12 kPa | 250 | 3 MPa | 0.3 | 0.47 |
| V Vallée de la Sionne | 0.7 | 0.2 | 0.002 | 3 kPa | 200 | 3 MPa | 0.3 | 0.49 |

## 5. Basal friction

**Coulomb.** This is Stomakhin §8's collision impulse: v_t' = v_t + μ v_n v_t/‖v_t‖, and it sticks
if ‖v_t‖ ≤ −μ v_n. It's applied at grid nodes below the terrain and again on the particles, as in
Stomakhin. The papers on avalanches (Li, Tonnel Eq. 15) use a stress-based law instead, τ_b = μ σ_n.
- **Info: the two forms agree in steady sliding.** The impulse acts on the normal velocity that gravity
  and stress create each step, so in steady sliding the two are equivalent. The offline energy-line
  test confirms it: 0.3024 against a set μ of 0.3.
- **Low: they can differ in transients.** The impulse depends on how much normal velocity exists at the
  node, so it depends on Δt and on which grid layer is inside the terrain. It acts on whole nodes
  below the surface, not on a contact area. So it can differ from the papers' law in transients and
  impacts.

**Medium: the Voellmy implementation.**
- **Constant depth.** The drag g|v|²/(ξ h) uses h = `slab_thickness`, a constant. In com1DFA h is the
  local flow thickness, so here the drag doesn't grow as the flow thins or shrink as it piles up.
- **Only on approaching nodes.** The drag is applied only where the node moves into the terrain
  (v_n < 0).
- **Source.** Tonnel 2023 (§2.3) only names Voellmy as an available model; the formula comes from the
  com1DFA documentation. com1DFA's **default** friction model is `samosATAuto`, not Voellmy;
  μ = 0.155 / ξ = 4000 are only the parameter defaults of its Voellmy option.
  - `01-theory.md`, `07 §2b`, `refs.md` and the code comment in `mpm_friction.wgsl` call those values
    "com1DFA's default". That should be reworded.

## 6. Energy line (Tonnel et al. 2023, §5.2 and App. A)

**Verified correct**
- **Energy height.** h_E = z̄ + v̄²/(2g), where the code uses the mean of |v|² (the kinetic energy),
  not |mean v|². That's correct for Eq. A4.
- **Fit.** The fitted slope against path gives −tan α.

**Medium: the path definition.** Eq. A4 is the mass average of each material point's own equation
(A3), so s is the **mass-averaged path length of the points**. The code instead integrates the
horizontal path of the **centre of mass**. For a flow that spreads laterally or along the slope, the
centre of mass travels a shorter path than the average point. So `μ_eff` is biased **high**, and the
"internal dissipation" (μ_eff − μ) is overestimated.
- **Fix:** accumulate per-particle horizontal travel |Δx_xy| in a mass-weighted sum in `mpm_splat`,
  using the same 64-bit lo/hi trick.

**Low: what μ_eff − μ means.** Tonnel's test assumes Coulomb friction is the only dissipation. Reading
μ_eff − μ as "internal dissipation" is only valid with the Coulomb basal law. With Voellmy, μ_eff also
contains the drag. The UI and the docs should say so.

## 7. Diagnostics

**Medium: the plastic-particle ratio.**
- **What the code counts.** `mpm_splat` counts particles whose `plastic_state` differs from its initial
  value, i.e. particles that have *ever* yielded.
- **What Li 2021 counts.** Li's 77.2 / 25.8 / 10.4 / 34.0 % for cases I–IV are the share of particles
  in a plastic state at t = 10 s, from the stress distribution in their Fig. 5.
- **Why it matters.** The code's metric is cumulative and only grows, so it reaches ~100 % for
  Stomakhin and 50 %+ for the sliding-slab preset. The code comment in `mpm_splat.wgsl` and the panel
  tooltip compare it with Li's numbers anyway.
- **Fix:** either count particles that yielded *in the last substep* (y ≥ 0 or δγ > 0 at the last
  return), or rename the readout to "ever yielded" and drop the comparison.

## 8. Numerics and parameters

- **Low: the timestep bound.** The CFL bound in `apply_material_preset` uses c = √(E/ρ). The P-wave
  speed √((λ+2μ)/ρ) is 1.16× higher at ν = 0.3, though the 0.1 safety factor hides it. Also, dt is only
  re-checked when a preset is applied, not when E or ρ is edited by hand.
- **Low: SVD precision.** The 3×3 SVD is computed from the eigen-decomposition of FᵀF in f32, which
  squares the condition number. It's adequate near the identity, where Stomakhin clamps σ and Cam
  Clay's elastic strains are ~10⁻³. It degrades for strongly distorted F under Drucker–Prager or
  Cam Clay.
- **Info: artificial boundaries.** The domain walls (zero outward velocity in the outer two nodes) and
  the band ceiling are artificial boundaries with no counterpart in the papers. They are documented.
- **Info: no snow cover is entrained.** Li 2021 states the same limitation explicitly.

## 9. Documentation

The docs are thorough and mostly match the code: formulas, factors, file layout, verification. These
items need fixing:

1. **com1DFA default** (see §5): the docs call Voellmy with μ 0.155 / ξ 4000 the default. Fix in
   `01-theory.md:285–295`, `07-constitutive-models.md:64–66`, `refs.md:66–67` and the comment in
   `mpm_friction.wgsl`.
2. **Drucker–Prager "cheaper than Cam Clay"**: contradicted by the benchmark (see §3).
3. **Plastic ratio "compared with Li 2021"**: the definitions differ (see §7).
4. **Preset notes:** they describe Li's results as if they were this model's behaviour (see §4). Add
   that they come from the paper, with a different flow rule and resolution.
5. **The issue's DOI for Tonnel 2023 is wrong.** It gives `10.5194/gmd-16-533-2023`; the paper is
   `10.5194/gmd-16-7013-2023`, which `refs.md` already has right.
6. **Klár 2016 and Wolper 2019 aren't in the paper folder**, so the two return mappings that cite them
   can't be checked against the source by a reader.

## 10. Recommendations, in order

1. **Implement the associative Cam Clay return** (Gaume Methods / Supplementary Note 1). Keep the
   fixed-p return as a switch, and compare the two on the sliding-slab and warm-shear presets.
2. **Rename or annotate the regime presets** as "Li 2021 parameters", not as regimes this solver
   reproduces at 12.5 m. Consider rescaling ξ with Δx as Li suggests.
3. **Fix the two diagnostics:** the plastic ratio (instantaneous yield) and the energy-line path
   (mass-averaged point path).
4. **Voellmy:** use a local flow depth, e.g. from the density raster or the particle count per column,
   instead of `slab_thickness`. Fix the com1DFA attribution.
5. **Documentation and guards:** list the Stomakhin J_P and hardening clamps and the Cam Clay guards as
   deviations, fix the Drucker–Prager cost claim, and re-check dt when E or ρ is edited.
6. **Put Klár 2016 and Wolper 2019 in the paper folder,** so the Drucker–Prager and fixed-p returns can
   be checked against the source.

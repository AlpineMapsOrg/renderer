# Implementation audit — final report

*Issue [#3](https://github.com/Emanum/webigeo/issues/3), 2026-10-03, `main` at `a7626e8e`.*

This report merges two independent audits of the same code:
- **[A]** [`report-implementation-audit.md`](report-implementation-audit.md), written with GitHub
  Copilot (GPT-5.6 Luna).
- **[B]** [`report-implementation-audit-opus.md`](report-implementation-audit-opus.md), written with
  Claude Opus 5.5. It was written *before* [A] was read, then compared with it.

Every finding below was re-checked against the source and, where possible, against the paper text.
The **Source** column says which audit found it.

**Papers checked against their text:** Hu 2018, Stomakhin 2013, Gaume 2018, Li 2020, Li 2021 and
Tonnel 2023, all from the project's paper folder. **Not available:** Klár 2016 and Wolper 2019 aren't
in the folder. Statements about them rely on the published closed form, or are marked unverified.

## 1. Bottom line

The **numerics that are implemented are correct.** The MLS-MPM transfers and force term, Stomakhin's
model, the Drucker–Prager cone and return, the Cam Clay yield function, invariants, hardening law and
initial state, and all Li 2021 preset values match the papers. Both audits agree on this.

Where the implementation departs from the papers:
1. the **flow rule** of Cam Clay,
2. the **resolution** the presets are used at,
3. the **Voellmy** closure,
4. **two diagnostics** that are compared with literature values they don't measure.

Nothing in the solver is a hidden bug. The two main issues are labelling and comparability.

## 2. Findings

### High

| # | Finding | Source |
|---|---|---|
| H1 | **Cam Clay uses a non-associative fixed-p return, but Gaume 2018 and both Li papers use an associative flow rule.** | both; the Li part is from [B] |
| H2 | **The Li 2021 regime presets run 25× coarser than Li, with an unregularised ξ.** | [B]; [A] only in general terms |

**H1, the flow rule.** Gaume 2018 (Methods, Eq. 12–13) defines the return as the projection minimising
‖τ − τ_tr‖ in the C⁻¹ norm. Li 2020 and Li 2021 state that they use an associative plastic flow rule
(Simo; Simo–Meschke).

`ccc_plasticity()` does something else. It returns to (p₀, 0) at the cap and (−βp₀, 0) at the tip, as
the associative rule would there. For shear it scales q at fixed p, so shear never produces volumetric
plastic strain and never changes α or p₀. Under the associative rule, shear on the tensile side of the
ellipse's crown, p < p₀(1−β)/2, dilates and softens, and shear on the compressive side compacts and
hardens.

[A] states the Gaume difference correctly. [B] adds that **Li's presets were calibrated with the
associative rule**, so the "sliding slab" and "warm shear" behaviour that depends on shear softening
can't be expected. The docs disclose the choice. The preset names and notes still describe Li's
results.

**H2, resolution.** Li 2021 runs at Δx = 0.5 m with 8 particles per cell, Δt = 2·10⁻³ s, 1.9 M
particles and a 1.05 m release depth (Li 2020: Δx = 0.05 m). The app runs at Δx = 12.5 m, so the
1.5 m slab is 0.12 cells thick, with ~300+ particles per cell. Li 2021 also suggests adjusting ξ with
the mesh size to keep the fracture energy constant; the code doesn't. [A]'s "12–16 m cells" is out of
date: the preset is 12.5 m.

### Medium

| # | Finding | Source |
|---|---|---|
| M1 | **Plastic-particle ratio isn't comparable to Li 2021.** The code counts "ever yielded" (`plastic_state ≠ initial`, cumulative). Li's 77.2 / 25.8 / 10.4 / 34.0 % is the share of particles in a plastic state at t = 10 s (Fig. 5). The code comment and the tooltip compare the two anyway. | [B] |
| M2 | **Energy line uses the centre of mass's own path.** Tonnel App. A (Eq. A4–A5) averages each point's own energy equation, so s is the mass-averaged point path, and the centre-of-mass path of a spreading flow is shorter. `μ_eff` is biased high, and so is the "internal dissipation" μ_eff − μ. | [B] |
| M3 | **Voellmy uses a constant reference depth** (`slab_thickness`) instead of the local flow thickness. The drag is applied only where v_n < 0 at nodes below the terrain. | both (depth); [B] (v_n gate) |
| M4 | **Voellmy is attributed to the wrong source.** Tonnel 2023 only *names* Voellmy and samosAT and gives no Voellmy formula; its tests use Coulomb. [A] says Tonnel "use[s] the depth-integrated resistance τ = μσ_n + ρg\|v\|²/ξ", which the paper text doesn't support. The docs call μ 0.155 / ξ 4000 "com1DFA's default". com1DFA's default model is `samosATAuto`; those values are only its Voellmy defaults. | [B]; [A] repeats the misattribution |
| M5 | **The terrain-band ceiling is an artificial boundary.** Particles are clamped under the band top, `(floor + layers − 2.5)·dx`. A quantitative runout study must show the ceiling stays inactive, or repeat the run with more layers. | [A]; [B] lists it as Info only. [A] is right to raise it |

### Low

| # | Finding | Source |
|---|---|---|
| L1 | **Guards that aren't in the papers:** Stomakhin's J_P and hardening factor are clamped to [0.05, 20], which caps compaction stiffening at 20×; the Cam Clay sinh argument is clamped to 20; σ is clamped to [10⁻³, 10³] before the log; and M and ξ have minimums. They're sensible but should be listed as deviations. | both |
| L2 | **Drucker–Prager has no friction-angle hardening** (Klár §5.4); `plastic_state` is a diagnostic only. | both |
| L3 | **"Drucker–Prager is cheaper than Cam Clay / the fallback if Cam Clay is too slow"** (shader header, docs) is contradicted by the M5 benchmark: 0.66 ms per substep against 0.64 ms. | [B] |
| L4 | **Coulomb is a velocity-impulse law** (Stomakhin §8), not the stress law τ = μσ_n of Li and Tonnel Eq. 15. They agree in steady sliding (offline energy line 0.3024 against a set 0.3), but differ in transients and impacts because the impulse depends on per-node normal velocity, Δt and which layer is inside the terrain. | [B]; [A] describes the impulse correctly but doesn't compare it |
| L5 | **The CFL bound uses √(E/ρ), not the P-wave speed** (1.16× higher at ν = 0.3), and dt is only re-checked when a preset is applied, not after hand edits. | [B] |
| L6 | **The SVD comes from the eigen-decomposition of FᵀF in f32,** which squares the condition number. It's fine near the identity, where Stomakhin and Cam Clay elastic strains stay; it's weaker for strongly distorted F. | [B] |

### Info and documentation

| # | Finding | Source |
|---|---|---|
| D1 | **Stomakhin's parameters belong to a different integrator.** Table 2 was tuned for semi-implicit integration with PIC/FLIP at α = 0.95. Explicit APIC is less dissipative, so "film snow" can look more energetic. | both |
| D2 | **Pressure is compressive-positive** (p = −K tr ε); papers with tension-positive mean stress look inverted unless the sign is converted. | [A] |
| D3 | **The issue's DOI for Tonnel 2023 (`gmd-16-533-2023`) is wrong.** It should be `10.5194/gmd-16-7013-2023`; `refs.md` already has it right. | [B] |
| D4 | **Klár 2016 and Wolper 2019 aren't in the paper folder**, so readers can't check the Drucker–Prager and fixed-p returns against the source. | [B] |
| D5 | **Total mass is normalised.** The simulated total mass isn't the physical slab mass until the scale is restored. The dynamics are invariant, but exported quantities would need rescaling. | [A] |
| D6 | **[A]'s line references have drifted in places.** For example, `mpm_common.wgsl:161-201` points at the `SUM_*` constants, not the fixed-point helpers; `mpm_g2p.wgsl:51-57` and `:77-84` are shifted; `mpm_material_drucker_prager.wgsl:84` is blank. [A] recommends recording the commit SHA, which this report does. | [B] checking [A] |

### Verified correct (both audits)

- **Transfers.** The quadratic B-spline kernel. P2G momentum `w(m v + (m C − Δt V⁰ (4/Δx²) τ)(x_i − x_p))`
  is Hu Eq. 15/18. G2P `C = (4/Δx²) Σ w v (x_i − x_p)ᵀ`; the world-unit and grid-unit asymmetry is
  intentional. `F ← (I + Δt C)F`, then plasticity, then advection, in Hu §4's order.
- **Stomakhin:** fixed-corotated τ, exponential hardening of μ and λ, singular-value clamp, and the
  J_P update conserving total J (Eq. 1–2, §7). The Table 2 defaults match.
- **Drucker–Prager:** Hencky elasticity, α = √(2/3)·2 sin φ/(3 − sin φ), δγ = ‖ε̂‖ + (3λ+2μ)/(2μ) tr ε α,
  and Cases I–III. The preset's sin φ = 3M/(6+M) conversion is right. Checked against the published
  form; the paper isn't in the folder.
- **Cam Clay:** p and q (Gaume Eq. 1–2), y (Eq. 3), p₀ = K sinh(ξ max(−ε_V^P, 0)) (Eq. 4), α as log
  volume change, the initial α from p₀ⁱⁿⁱ, and the cap and tip returns with correct signs.
- **Presets:** all five Cam Clay presets equal Li 2021 Table 1, including ρ, E, ν and basal μ.
- **Energy line:** the energy height uses the mean of |v|² (kinetic energy), as Eq. A4 needs.

## 3. Discussion: how the two audits compare

**Where they agree.** The structural verdicts are the same: the transfers match, Stomakhin matches up
to its clamps, Drucker–Prager is a fixed cone without hardening, Cam Clay has Gaume's surface but not
Gaume's flow rule, Voellmy is a depth-averaged closure, and nothing is validated against a real
avalanche. Two independent reads reaching the same formula-level conclusions is good evidence that the
implemented maths is right.

**What [A] does better.**
- It traces equations to code line by line, with a verification matrix ("what each test proves and
  does not prove").
- It treats the terrain band ceiling as a real validity condition (M5).
- It proposes concrete process improvements: convergence tests at two Δx values, GPU "golden"
  single-particle readbacks checked against the Python ports, boundary-activation tests, and recording
  the commit SHA.

[B] has none of these, and they belong in the plan.

**What [B] adds.**
- **Comparability problems.** [B] checks the outputs against what the papers actually measured, not
  just the equations. That finds two diagnostics compared with quantities they don't measure: the
  plastic ratio (M1) and the energy-line path (M2).
- **Quantified context.** It checks Li's numerics (Δx, particles per cell, Δt, ξ regularisation) and
  the fact that Li's calibration used the associative rule (H1, H2). That shows the regime presets are
  labels, not reproductions.
- **Claims checked against measurements and paper text.** The Drucker–Prager cost claim (L3), the
  com1DFA attribution (M4) and the issue's DOI (D3) come from this.

**Corrections.**
- **To [A]:**
  - The Voellmy formula isn't in Tonnel 2023 (M4).
  - The cell size is 12.5 m, not 12–16 m.
  - Several line references have drifted (D6).
  - Its "parameter mapping, not calibration" verdict on the presets understates the problem. The
    presets come from a model with a different flow rule and 25× finer resolution.
- **To [B]:**
  - The band ceiling deserved more than "Info" (M5).
  - It missed [A]'s sign-convention note (D2) and the process recommendations.
  - Its Drucker–Prager and Wolper statements rest on memory, not the papers, and must stay marked as
    unverified until the PDFs are added.

## 4. Recommendations (merged, in priority order)

1. **Cam Clay flow rule:** implement Gaume's associative return (Methods, Eq. 12–13, Supplementary
   Note 1) behind a switch, keep the fixed-p return, and compare them on the sliding-slab and
   warm-shear presets. *(H1)*
2. **Make preset labels honest:** call them "Li 2021 parameters" and say in the notes that the paper's
   behaviour came from an associative rule at Δx = 0.5 m. Consider scaling ξ with Δx as Li suggests.
   *(H1, H2)*
3. **Fix the diagnostics:** count plastic particles as yielded *in the last substep*, or rename the
   readout to "ever yielded" and drop the Li comparison. Accumulate the mass-averaged per-particle
   horizontal path for the energy line. Note in the UI that μ_eff − μ means internal dissipation only
   under the Coulomb basal law. *(M1, M2)*
4. **Voellmy:** use a local flow depth from the particle count per column instead of `slab_thickness`.
   Correct the attribution in `01-theory.md`, `07 §2b`, `refs.md` and `mpm_friction.wgsl`: com1DFA
   offers Voellmy, but its default is samosAT. *(M3, M4)*
5. **Tests ([A]):**
   - convergence at two Δx values and particles-per-cell densities;
   - GPU golden single-particle readbacks compared with the Python ports;
   - a run that shows the band ceiling stays inactive, plus a DEM-cell crossing case. *(M5)*
6. **Housekeeping:**
   - document the guards (L1);
   - fix the Drucker–Prager cost claim (L3);
   - re-check dt after hand edits and use the P-wave speed (L5);
   - add Klár 2016 and Wolper 2019 to the paper folder (D4);
   - fix the DOI in the issue (D3);
   - record the commit SHA in future reports (D6).
7. **Separate feature work, not bugs:** validation against com1DFA or Flow-Py, entrainment, and 3D
   particle rendering.

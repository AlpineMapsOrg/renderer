# Claude Session Log

Running log of Claude Code sessions on this repo: what was asked, what was decided, what
was actually verified, and what was left open.

**Convention.** Newest session at the top. One `##` section per session, dated. Within a
session, one `###` per distinct request. Record *decisions and their reasons* and
*verification evidence* — not a keystroke-by-keystroke transcript. Explicitly separate
"verified" from "assumed"; a log that overstates confidence is worse than no log.

This file is written by Claude and is a reconstruction from session context, not a verbatim
capture of the terminal.

---

## 2026-09-13

### 1. Proposal v3.0 gap analysis — constitutive & friction models

Read the Notion proposal (now **v3.0**, edited today) and the new *Snow/Avalanche Sim Paper
Summaries* page, then checked each named model against the shader source.

**v3.0 changes vs v1.0:** constitutive model becomes a configurable choice (Stomakhin [5] or
successors Gaume CCC [11], Li et al. [12,13]) with per-regime presets; basal friction is
explicitly a separate simple Coulomb model using terrain normals; entrainable material is a
new goal; comparison against non-real-time models is a new final stage.

**Verified implemented:** Stomakhin 2013 in full (fixed-corotated elasticity, SV box clamp,
exponential hardening, paper defaults). Coulomb basal friction at grid + particle level using
terrain normals — exactly what v3.0 asks for, though μ defaults to 0.4 where Li 2021 uses
0.47 on real terrain.

**Not implemented:** CCC, Li regime parameters/presets, Drucker–Prager, Voellmy, any model
*switch* at all (Stomakhin is called unconditionally), entrainment, energy-line validation.

Wrote `mpm-mls-doc/07-constitutive-models.md`: the gap table, the design for exchangeable
models, and an ordered next-steps list. Key design decision: follow the codebase precedent
in `ComputeAvalancheTrajectoriesNode` — C++ enum → `u32` uniform → runtime `switch` in WGSL
→ ImGui combo — rather than compile-time `///if` variants, because the preprocessor's
defines are global and a uniform branch has no divergence cost. Two *separate* enums
(constitutive vs basal friction) so internal and basal friction cannot be conflated, which
both the proposal and the paper summaries warn about. Per-particle state stays one scalar
for all three candidate models, so `Particle` does not change (`jp` → `plastic_state`).

Next-steps order deliberately puts the no-physics refactor first, then Drucker–Prager
(closed-form, validates Hencky + the dispatcher) *before* CCC (implicit ellipse return
mapping, the hard part, and the documented fallback target if CCC is too slow).

### 2. "yes start with step 1" — model dispatcher refactor, no new physics

Made the constitutive model and basal friction exchangeable without changing behaviour.

**Shaders.** Moved the Stomakhin model out of `mpm_common.wgsl` into
`mpm_material_stomakhin.wgsl` (`stomakhin_initial_state/stress/plasticity`). Added
`mpm_material.wgsl` — three `switch settings.constitutive_model` dispatchers — and
`mpm_friction.wgsl` — Coulomb behind `switch settings.basal_friction_model`. `PlasticReturn`
(the return-mapping result type) lives in `mpm_common` as shared interface. `p2g`/`g2p`/`seed`
call the dispatchers; `grid_update`/`g2p` include the friction module. Renamed `Particle.jp` →
`plastic_state` everywhere. `mpm_common` no longer injects material/collision code into
kernels that never used it (`prepare`, `splat`, `rasterize`, `clear_grid`).

One interface addition beyond the plan: `material_initial_state()`. The initial plastic state
is model-dependent (Jp = 1 for Stomakhin, εᵥᵖ = 0 for CCC/DP), so `mpm_seed` cannot hard-code
`1.0`.

**C++.** `enum ConstitutiveModel { STOMAKHIN_2013 }`, `enum BasalFrictionModel { COULOMB }`;
settings fields; uniform +16 B (144 → 160, `static_assert` updated); serialised as ints;
`Combo` in both the node renderer and the sidebar, with Stomakhin's parameters shown under an
`if` on the active model. Three shaders added to the CMake resource list.

**Verification — the point of step 1 is that nothing changed:**
- `tint`: 8/8 pass with the modular includes (the preprocessor is pragma-once, confirmed in
  `ShaderPreprocessor.cpp:199`, so modules can `///use mpm_common` for themselves).
- **Resolved-shader diff against git HEAD** (`scripts/check_refactor_preserving.py`): resolves
  includes on both sides, extracts every top-level definition, normalises the intentional
  renames, folds the dispatcher wrappers away. Result: every retained function body identical
  modulo renames; the only definitions that vanished from a kernel are ones it never referenced;
  `MpmSettings` +4 fields exactly. Two rounds of false positives were bugs in the checker
  (renaming a call site also renamed the dispatcher's *definition* and clobbered the real one in
  the dict; dict keys weren't renamed alongside bodies), not in the refactor.
- Runtime: booted with the MPM graph, all 8 pipelines created on Metal, no Dawn errors.
  Temporary boot/log changes reverted.

Docs updated: 02 (files, enums), 03 (modules section), 04 (160 B, `plastic_state`), 05 (the
"adding a constitutive model" recipe), 06 (§4b resolved-shader diff), 07 (status reconciled,
step 1 ticked), refs.

### 3. "yes continue with step 2" — μ → 0.47, Voellmy basal friction

**Changes.** `terrain_friction` default 0.4 → **0.47** (Li et al. 2021 Table 1, real terrain);
preset updated. `BasalFrictionModel::VOELLMY = 1` with `voellmy_friction()` in
`mpm_friction.wgsl`: Coulomb plus the turbulent term, `τ = μσₙ + ρg|v|²/ξ` → deceleration
`g|v|²/(ξ·h)`. Used `h = slab_thickness` as the reference depth because that is the
convention both com1DFA and `ComputeAvalancheTrajectoriesNode` use (the latter hard-codes
`h = 1 m`), so ξ stays in literature units — default 4000, with a note that the conventional
pairing is a lower μ ≈ 0.155. `voellmy_xi` took the spare `_pad_a` slot in the uniform, so no
layout growth. Combos in both panels gained "Voellmy" and a ξ slider shown only when active.

**A subtlety worth recording.** `resolve_terrain_collision()` is called at both the grid
and the particle level. Coulomb is a contact *impulse* depending on `vn`, which is ≈ 0 by the
particle-level call, so double application is benign. Voellmy's drag depends on `|v_t|²`, not
`vn`, so the same double call would apply it **twice per substep**. Added an
`apply_basal_drag` flag: `true` in `mpm_grid_update`, `false` in `mpm_g2p`. Coulomb ignores
it. This distinction — impulse terms are level-agnostic, velocity-dependent drag is grid-only
— is now written into the friction module header and `05-tuning.md` for the next model.

**Verification.**
- New `scripts/test_friction.py`: verbatim port, point mass on an inclined plane stepped the
  way `mpm_grid_update` does it. Voellmy reaches the analytic terminal velocity
  `√(ξh(sinθ − μcosθ))` to **0.085 %** (explicit-Euler lag); Coulomb's slope acceleration
  matches `g(sinθ − μcosθ)` to 3 dp (regression for the untouched path); both stick below
  atan(μ); `apply_basal_drag = false` reduces *exactly* to Coulomb; a 500 m/s / dt = 1 s
  reversal guard holds.
- `check_refactor_preserving.py` extended (flag stripped at call sites, Voellmy defs
  allowed) and re-run against pre-step-1 HEAD: still behaviour-preserving for the Coulomb
  path with steps 1 + 2 combined.
- `tint` 8/8; boot with MPM graph, 8 pipelines on Metal, no Dawn errors; temporaries reverted.

Docs: 01 (Voellmy formula + terminal velocity), 02, 03 (flagged call sites, seed init), 04,
05 (params table, the impulse-vs-drag rule), 06 (§4c friction bench), 07 (step 2 ticked, §2b
rewritten), README, refs (Tonnel 2023, Li 2021 entries).

### 4. "continue with step 3" — Drucker–Prager (Klár 2016)

First real second *constitutive* model, deliberately before CCC: its return mapping is a
closed-form projection, and it brings in the Hencky elasticity CCC will reuse.

**Implementation.** `mpm_material_drucker_prager.wgsl`, `ConstitutiveModel::DRUCKER_PRAGER = 1`.
Hencky strain `ε = log Σ`, Kirchhoff stress `τ = 2με + λ tr(ε)` in the principal frame,
`P Fᵀ = U diag(τ) Uᵀ`. Yield cone `‖dev τ‖ + α tr τ ≤ 0`, `α = √(2/3)·2sinφ/(3−sinφ)`
precomputed CPU-side from a `dp_friction_angle` setting (default 30°). Return mapping is
Klár §5.3 Cases I/II/III. `plastic_state` accumulates δγ; hardening of φ left out.

**Two design changes against the plan in `07`, both recorded there.** (a) E/ν are *shared*
across models instead of duplicated as `hencky_*` — two stiffness knobs that silently
disagree is worse than one, the CFL readout keys off the shared E, and presets are the
right place for per-model defaults. So DP needed exactly one field (`dp_alpha`, last pad
slot, uniform still 160 B). (b) `DRUCKER_PRAGER = 1` not 2, so the combo stays contiguous
until CCC exists.

**Bug caught by the test before it shipped.** The first draft guarded Case II with
`‖ε̂‖ = 0 OR tr ε > 0`, which sends *pure hydrostatic compression* to the cone apex and
drops all elastic strain. Hydrostatic compression has zero deviatoric strain but sits on
the cone's axis, inside the surface — it must stay elastic. With `tr ≤ 0` the second term
of δγ is ≤ 0, so `δγ > 0` already implies `‖ε̂‖ > 0`; the division guard was unnecessary
and the condition is `tr > 0` alone. Test case 1 in `test_material_dp.py` pins it.

**Verification.**
- `test_material_dp.py`: 12 checks. The substantive one — every Case III projection lands
  *on* the cone, `|y| < 1.5e-10` over 500 random gradients; never outside it. Idempotence
  check needed loosening from "case label is I" to "state doesn't move": an on-cone state
  has `y ≈ +1e-12` and the re-projection fires Case III with δγ ≈ 1e-15. Test strictness,
  not physics.
- `test_mpm.py` parametrised by `MPM_MODEL`, reusing the DP port. Stomakhin regression
  unchanged (jp 0.7456). DP on the same drop: mean z 3.65 and still falling vs 4.00 stable;
  max|v| 2.7 m/s still spreading vs 0.06 at rest; plastic strain growing 0.25 → 0.92 vs
  saturated. Cohesive piles and stops, cohesionless keeps spreading — the expected physical
  difference, with no tuning.
- `tint` 8/8; 8 pipelines on Metal; temporaries reverted.

Docs: 01 (§4b Hencky + cone), 02, 03, 04 (no pad slots left → next field is 176 B), 05
(shared-E rule), 06 (§4d), 07 (§2d incl. the bug and the design changes; step 3 ticked),
README, refs (Klár 2016).

### 5. "commit step 3 then continue with step 4" — Cohesive Cam Clay (Gaume 2018)

Step 3 committed as `be83247d`. Then the model the proposal's paper survey recommends.

**Implementation.** `mpm_material_ccc.wgsl`, `ConstitutiveModel::COHESIVE_CAM_CLAY = 2`.
Hencky elasticity (shared with DP). Gaume's ellipse `(1+2β)q² + M²(p+βp₀)(p−p₀) ≤ 0` with
`p = −K tr ε`, `q = √(3/2)·2μ‖dev ε‖`; hardening `p₀ = K sinh(ξ max(−α,0))`, α the
plastic volumetric strain. `plastic_state` = α, initialised to `−asinh(p₀ⁱⁿⁱ/K)/ξ` so
`p₀(α₀) = p₀ⁱⁿⁱ` — the reason `material_initial_state()` had to exist. Four new uniform
fields; **uniform 160 → 176 B**, no pad slots left. Defaults are Li 2021 Case V (the
real-avalanche back-calculation): M 0.7, β 0.2, ξ 0.002, p₀ 3 kPa.

**The return-mapping decision, made explicit.** Gaume describes an associative flow rule,
which needs a per-particle Newton solve on the ellipse. Implemented instead is the
three-case projection of Wolper et al. 2019 (NACC) — the same group's own implementation
of this yield surface and hardening law: cap → `(p₀,0)` + harden; tensile tip → `(−βp₀,0)`
+ soften; shear → `q` onto the ellipse at fixed p. Explicit (old p₀ for the projection).
Both papers cited; the docs say which is which; swapping in an associative return only
touches `ccc_plasticity()`. Checked on paper that Case 3's `q_new/q` division is safe: with
p in range the ellipse term is ≤ 0, so `y > 0 ⟹ q > 0`.

**Verification.**
- `test_material_ccc.py`, 12 checks — the yield surface and hardening law are closed-form,
  so every projection is checked directly. α₀ round-trips exactly; Case 3 lands on the
  ellipse (`|y| ≈ 7e-8` on a 1e7 scale) *at the same p*; Case 1 returns to `(p₀,0)` and
  hardens (3000 → 3012); Case 2 returns to `(−βp₀,0)` and softens; 500 random gradients
  never leave the surface; repeated tension softens monotonically to **exactly p₀ = 0**
  (fracture); repeated compression hardens until admissible then stops; β = 0 has no
  tensile strength; all five Li 2021 Table 1 cases produce finite α₀ that round-trip at
  E = 3 MPa. Two first-run failures were test strains too small to reach the surface
  (0.2 % expansion = −467 Pa vs 600 Pa strength) — inputs, not model.
- `test_mpm.py` with `MPM_MODEL=ccc`. **The parameter → behaviour link works**: weak Case V
  flattens to a pancake (mean z 3.02) because a 14 kPa impact is far above the 3 kPa cap
  and Cam-Clay *loses* shear strength above p₀ — the opposite of DP's cone; strong Case III
  (42 kPa) piles at 4.13 alongside Stomakhin's 4.00. Same code, four parameters. Both come
  to rest; DP doesn't.
- `tint` 8/8; 8 pipelines on Metal; temporaries reverted.

Docs: 01 (§4c, incl. the "softer than DP above p₀" point), 02, 03, 04 (176 B), 05
(params row — p₀ is the knob), 06 (§4e with the four-way drop comparison), 07 (§2e,
step 4 ticked, v3.0 table: all three named models done), README, refs (Wolper 2019).

**Not done in step 4, flagged in 07:** benchmark it. One SVD + scalars per particle, same
order as Stomakhin, so the expectation is fine — but that is an expectation.

### 6. "commit step 4 then continue with step 5" — regime presets + diagnostics readback

Step 4 committed as `90991efa`.

**Presets.** `AvalanchePanel::MaterialPreset`, seven entries: Stomakhin 2013; Li 2021
Cases I–IV and V; a Drucker–Prager cold-dense fallback (φ ≈ 13.3° from M = 0.5). Presets
*write the ordinary settings* — model, E, ν, ρ, μ, model params — nothing more, so there is
one source of truth and a preset can be edited afterwards. Applying one forces a reseed and
pulls dt under the new CFL bound (3 MPa is ~6× the wave speed of 0.14 MPa; the first run
would otherwise explode). Hand edits drop the combo to "(custom)".

**Bug found while doing it.** Switching the material combo did not reseed. `plastic_state`
is model-specific: Stomakhin's `Jp = 1` read by Cam-Clay is `α = 1` → fully softened,
`p₀ = 0`, a material that carries no stress. Latent since step 3. Both panels now force
`reset_on_next_run` on any model change.

**Readback.** `SimState` +`plastic_particles` (24 B); counted once per run in `mpm_splat`
as particles whose state left `material_initial_state()`. The state buffer is now
allocated once in the constructor — fixed-size, and an async readback in flight across an
`ensure_resources()` reallocation would be a use-after-free. Per-run counters zeroed by a
2-slot `write()` before non-reset runs so the terrain scan and seed count survive.
`read_back_state()` fires from the work-done callback into `last_state()`; sidebar shows
"N particles, X % plastic, max V m/s, terrain A–B m", plus a warning when 0 seeded.

One edit went wrong and was caught on re-read: inserting the `else { reset_run_counters() }`
split the block so the location log moved into the non-reset branch. Fixed before build.

**Verification.** `tint` 8/8, build clean. Then the real one — the readback is a path no
offline check covers — by temporarily auto-running the graph on load: 131072 active,
968 plastic (0.7 %), max 1.57 m/s (≈ g·sinθ·t for 0.24 s), terrain 1303–2060 m. Every
number plausible; first real-terrain diagnostic the solver has produced. Temporaries
reverted, tree clean.

Docs: 02, 03, 04 (SimState 24 B, allocation rule), 05 ("start from a preset", the
model-switch rule, two new failure-mode rows), 06 (§4f, bug #6), 07 (§2f, step 5 ticked;
v3.0 table now shows the whole material layer done), README.

### 7. "step 6" — energy-line validation (com1DFA §5.2)

Step 5 committed as `8779515d` first.

**What it measures.** Energy height `h_E = z_com + v̄²/2g` drops by exactly μ per
*horizontal* metre under Coulomb friction, whatever the slope, because the friction work
`μ m g cosθ ds` has `cosθ ds` = horizontal increment. So the slope of `h_E` vs horizontal
path is `−μ_eff`, and `μ_eff − μ` is internal dissipation. A force-balance check of the
whole loop that needs no real avalanche.

**Offline** (`test_energy_line.py`, `test_mpm.py` terrain generalised to a slope with the
flat regression unchanged at 150.00 / 3.00 / 0.7456). Slab on a 30° plane, two runs:
μ = 0 → `μ_eff = 0.0026` (energy conserved); μ = 0.3 → `μ_eff = 0.305`; **difference 0.3024
vs 0.3 expected** — under 1 %. The strongest single validation so far.

The first version *failed* and the failure was mine: terrain rises with +x so downhill is
−x, and the slab was seeded 2–7 m from the wall at x = 2. It slid into the wall and
reported `μ_eff = 0.91` on a frictionless plane and a *negative* basal contribution. Seeded
at the top of the slope instead; both runs then reach the same 87 % of ideal free-slide
distance, so the shortfall is startup geometry.

**On device.** `mpm_splat` sums position and `|v|²`; `read_back_state()` appends an
`EnergySample`; `energy_line_friction()` fits over the sliding regime (past 10 % of total
path — the seeded slab compacts before sliding). Sidebar shows `μ_eff`, set μ, difference,
sparkline.

**Bug caught by looking at the raw samples, not by a test.** First GPU version summed
`value/N × scale` to keep a mean inside an `i32`. Per particle that is
`floor(v²/131072 × 10⁴)` → 0 below 3.6 m/s. KE read **0 for the first three runs and ~30 %
low after** (sample 8: 9.79 vs 14.78 corrected). Replaced with real 64-bit sums: lo/hi `u32`
pairs, carry detected as `atomicAdd(lo, x) > 0xFFFFFFFF − x`. `SimState` 24 → 56 B.
On device after the fix: KE resolved from run 1; `μ_eff` 0.65 → 0.61 converging, ~0.59
over the last window vs μ = 0.47 — ~0.12 internal dissipation on real terrain, where the
smooth plane gave 0.003. Temporaries reverted; `NodeGraphPanel.cpp` back to committed.

Docs: 02 (API), 03 (the lo/hi carry trick), 04 (SimState 56 B, why 64-bit), 05 (reading
`μ_eff`), 06 (§4g, bug #7), 07 (§2g, step 6 ticked), README.

### 8. "there is still an issue that the simulation doesn't cool down" — fixed-point energy pump

Screenshot: Sliding slab (CCC), 127 s, 100 % plastic, max speed 17 526 m/s, `μ_eff` 0.31
and falling. The energy-line readout from step 6 was the instrument that made this
visible as *energy creation* rather than "it's fast".

**Facts established on device** (a temporary harness in `AvalanchePanel::draw()` applying
a preset, auto-playing and logging `t / vmax / plastic / μ_eff / path / z / h_E` per run):
model-independent (Stomakhin, DP, CCC all blow up); particle-count-independent; the energy
injected per *step* is independent of dt (dt = 0.005 blew up sooner); no particle ever
flies or sinks — it is purely tangential; dropping the APIC `C` from P2G removes it, halving
`C` delays it 10 → 55 s, RPIC (rotation-only `C`) and a `|C·dpos| ≤ |v|` cap delay it
without removing it. So `C` looked like the culprit and PIC would have "fixed" it — at the
cost of a solver that creeps at 4 m/s.

**Ruled out** (each tested, none changed anything): particle-level speed clamps, a
strain-rate limit on the F update, a contact band and a two-sided terrain BC, smoothed
terrain normals, CFL.

**What found it.** All offline plane tests were at `dx = 1`. A vectorised numpy copy of
the loop at *device* scale (`dx = 12.5`, 1.5 m slab = 0.12 cells, dt = 0.01, 4 k
particles, 35° plane, μ = 0.47) was still exact against the rigid block: 37.00 vs 37.0 m/s
at 20 s. Adding one line — emulate the shader's `i32(x · 1e4)` truncation per P2G
contribution — reproduced the blow-up: 161 m/s mean, node velocities > 1000 m/s, same
exponential shape as the device. Mechanism: truncation loses up to one unit per
contribution; for a mass contribution `w = 1e-3` that is ~10 %, for its momentum `w·v` at
10 m/s ~1 %, so `momentum/mass` is biased *high* at every low-weight node, per step,
independent of dt. A slab 0.12 cells thick gives its third z layer exactly such weights
for every particle, and APIC's `C` carries the excess back into the particles. (First
attempt at this offline reproduction had its own bug — `svd` returns `Vh`, I transposed it
— which produced a rotation matrix for F and a fake "elastic instability"; caught by
printing F.)

**Fix** (`mpm_common.wgsl`, `mpm_p2g.wgsl`, `mpm_clear_grid.wgsl`, `mpm_grid_update.wgsl`,
`GRID_NODE_STRIDE_U32` 4 → 5): round instead of truncate; mass in its own 64-bit `u32`
lo/hi pair at 2⁻²⁰ (same carry trick as `SimState`), momentum unchanged at 1e-4 in an
`i32`, now with the range limit stated (particles-per-node × speed < 2·10⁵). Emulated
offline: shader scheme −0.1 % vs block; rounding alone still drifted +2 %, truncation with
a fine mass was 8 % *slow* (momentum truncation is dissipative), hence both changes.

**On device after the fix**, same harness: Stomakhin — vmax 21–26 m/s for all 144 s,
`μ_eff` 0.58 → 0.49 (basal 0.47), stops after 380 m; Sliding slab CCC — vmax ≤ 25 m/s,
plastic fraction 25 → 55 % (was 100 %), `μ_eff` 0.60 → 0.49, stops after 340 m. Residual:
~90 of 131 k particles keep moving at > 10 m/s after the mass has stopped — lone particles
on a coarse grid see little basal friction; noted in 05 as cosmetic.

New: `scripts/test_sheet_fixed_point.py` (regression test, emulates both schemes). All
TEMP harness/instrumentation reverted; `SimState` back to 14 u32. Offline suite re-run:
`test_mpm.py` ×3, `test_energy_line.py`, `test_sheet_fixed_point.py` all OK.
Docs: 03 (P2G/grid pseudo-code), 04 (GridNode 20 B, "round, don't truncate"), 05 (two
new failure-mode rows), 06 (§4h, bug #8 with the detours), 07 (§2g addendum, step 6).

### 9. "work instead on getting the grid correct" — domain size analysis, then E + B

Step 7 (entrainment) was started and abandoned at the user's request; partial edits
reverted. Task: the simulation is boxed into a fixed area and clamps at the walls;
research fixes, list options, estimate the non-MPM code each touches (less = easier
upstream approval).

**Analysis** (`mpm-mls-doc/08-domain-size-options.md`). Two boxes: the terrain *region*
(`GeoRegionNode.extent` 2500 m, upstream limit 8192 px stitched ≈ 100 km at z15) and the
MPM *domain* (1600 m, 128³, dx 12.5). The domain was small because the grid was a dense
`res³` box while the snow lives in 2–3 of 128 layers — 98 % air, cleared and updated every
substep. Faces are frictionless walls (`grid_update` zeroes outflow, `g2p` clamps). Two
facts make fixes cheap: the grid holds no state between substeps, and `rerun()` chains
down the graph so the overlay re-reads the domain aabb every run. Options: A bigger dense
grid (settings only, still `res³`), B terrain-following band grid (K layers per column,
~150 MPM lines, 0 upstream), C sparse/blocked (rejected: hash table + activation in WGSL
for reach nobody needs), D moving window (0 upstream, ~80 MPM lines, stateless grid makes
it free), E bigger region (settings). All options: **0 lines outside files we own**.

**Measured** with a temporary harness on the M5 (reverted): per run of 24 substeps,
`ms ≈ 9 + 25·(nodes/10⁶) + 0.38·(particles/10³)`. Preset 116 ms; 512² × 16 band ≈ 157 ms
(real time = 240 ms); particles cap real time at ~550 k.

**Implemented E + B.** `mpm_common`: z grid coordinate is now absolute `altitude / dx`;
`column_floor[]` (binding 8) written by `mpm_prepare` as `floor(terrain/dx) − 2`;
`grid_slot(node)` maps a stencil node through its own column's floor and returns −1
outside the band; P2G/G2P skip those; `grid_update` iterates `(x, y, layer)` and uses
`column_node_world()`; `g2p` clamps particles below the band ceiling
`(floor + layers − 2.5)·dx`. `grid_resolution_z` → `grid_layers` (default 16),
`MAX_GRID_RESOLUTION` → `_XY` 512 and `MAX_GRID_LAYERS` 64; both panels, JSON key,
scenario table (region 8000 m, domain 4000 m, 320 nodes → dx 12.5), preset JSON (raster
1024). `domain_base_altitude()` gone; `min/max_altitude` readout only.

**Verified.** Offline: the sheet test with the band emulated (8 layers) matches the
dense grid exactly. Device, Breite Ries 144 s, band 320² × 16 over 4 km vs dense 128³
over 1.6 km: path 378.3 vs 380.7 m, z_com 1757.6 vs 1756.5, `μ_eff` 0.4904 vs 0.4903,
100 vs 116 ms per run. So the flow really does stop at 1757 m on its own (slope under
tan⁻¹ 0.47 ≈ 25°) — it was not the wall.

Docs: 02, 03 (prepare, grid update, stencil), 04 (band section, buffer sizes), 05
(settings table, failure modes, cost), 06 (§4i), 08 (marked implemented), README.
Committed by the user as `f02ad59a` "Extend terrain".

## 2026-09-14

### 1. "the simulation runs in the same thread as the normal rendering - can we separate this?"

Diagnosis first, with a temporary fps harness (reverted): 13–25 fps while playing. Not a
thread problem — the CPU side of a run is microseconds of command encoding and the
work-done callback. WebGPU has a single queue; a run submitted as one ~100 ms command
buffer parks the next frame's render behind it. A CPU thread changes nothing; a second
device would give a second queue but cannot share the overlay texture or the terrain.

Fix: `MpmSolverNode::submit_chunk()` — the run is submitted in chunks of
`substeps_per_submit` substeps (first chunk clears the raster and does prepare/seed on a
reset, last chunk splats and rasterises), re-armed from the queue's work-done callback.
One chunk in flight left the queue idle every other frame (the callback lands two frames
after submission), so two are kept in flight with a counter deciding when the run is
complete. Measured (M5, preset, vsync 60): one submit 25 fps / 2.0× real time; chunks of
2 → **60 fps / 1.65×**; 4 → 53 fps / 1.8×. Default 2, setting in both panels ("Substeps
per frame"), JSON key. Docs: 02, 05 (setting, "frame rate vs simulation speed").

## 2026-09-06

### 1. "Make a folder mpm-mls-doc and document ... for my final report"

Created `mpm-mls-doc/` — implementation notes in an informal, technical register (own-notes
style, not a paper), sized for reuse in the final report.

- `README.md` — index, 60-second orientation, honest status/gaps
- `01-theory.md` — FLIP -> MPM -> MLS-MPM -> snow model; only the maths actually implemented,
  with pointers into the papers; a table of every deliberate deviation from the literature
- `02-files-and-api.md` — files added/changed, classes, methods, the null-socket gotcha
- `03-shaders.md` — the eight kernels one at a time, dispatch order, why one compute pass
- `04-data-layout.md` — struct layouts, fixed-point atomics, the mass-normalisation
  argument, coordinate conventions, uniform alignment rules
- `05-tuning.md` — what each knob does, failure-mode table, the resolution problem, how to
  extend (incl. where CK-MPM and the 3D renderer would go)
- `06-verification.md` — what was tested vs what wasn't, plus the bug post-mortems
- `refs.md` — bibliography, numbering matching the Notion proposal
- `scripts/` — `validate_wgsl.py`, `test_svd.py`, `test_mpm.py`

Note on "quotes": interpreted as *citations*, not verbatim quotations — each section points
at the specific paper/section/equation it follows rather than reproducing text. Better for a
report anyway, since the wording has to be the author's own.

The verification scripts had been lost when the session scratchpad was cleared overnight, so
they were rewritten into `mpm-mls-doc/scripts/` with repo-relative paths and **re-run** to
confirm the documented numbers rather than quoting them from memory: 8/8 kernels pass
`tint`; SVD exact over 700+ matrices; MPM loop conserves mass at 150.00, rests at min z 3.00,
mean jp 0.7456.


## 2026-08-29

Model: Claude Opus 5 (repo overview answered by Sonnet 5 before the model switch).

### 1. "Can you give me an overview of the repo"

Explored the tree and [README.md](README.md). Summary of what this is:

AlpineMaps.org / weBIGeo mono-repo — C++ renderer for large-scale, tile-based alpine
geodata, with two frontends over a shared core:

- `app/` — Qt Quick/QML + OpenGL, the original alpinemaps.org client
- `apps/webgpu_app/` — weBIGeo: WebGPU renderer, ImGui UI, GPU compute node graph
- `nucleus/` — shared core (tiles, camera, avalanche report data, picker, vector tiles)
- `gl_engine/` — OpenGL engine for `app`
- `webgpu/` — `base/` (RAII wrappers, WGSL preprocessor, resource registry),
  `engine/` (renderer), `compute/` (node graph + WGSL kernels)
- `plain_renderer/`, `unittests/`, `extern/`, `docs/`

Note the README's mermaid diagram is slightly stale: it shows `webgpu_engine` and
`webgpu_compute` as separate top-level dirs; on disk they are merged under `webgpu/`.

Conventions: CamelCase classes, snake_case methods/vars, `m_` private prefix, namespaces
mirror folders, 4-space indent, `.clang-format` provided.

### 2. "Check my notion page … Is a good way to do this in apps/webgpu_app/compute/nodes"

Read the Notion proposal *Real-Time Avalanche Simulation and Visualisation in weBIGeo*
(MLS-MPM snow solver + a rendering technique, VisComp WS26).

**Answer: the architecture fits well, but `apps/webgpu_app/compute/nodes/` is the wrong
half of it.** That directory holds only thin ImGui/imnodes presentation wrappers
(`*NodeRenderer`), registered by `dynamic_cast` in `NodeRendererFactory::create()`. No GPU
state or compute logic lives there.

The solver belongs in `webgpu/compute/nodes/` (namespace `webgpu_compute::nodes`), where
`Node` subclasses own GPU buffers/pipelines and implement `run_impl()`. Two close
templates: `ComputeAvalancheTrajectoriesNode` (settings/uniform struct pattern) and
`IterativeSimulationNode` (already does a bounded multi-dispatch GPU loop decoupled from
the render loop — exactly the shape an MPM substep loop needs).

Also flagged: the proposal's "Particle Renderer" is not a compute-node concern.
`OverlayRenderNode` is a 2D overlay bridge and cannot express 3D particles; that needs a
new engine-side Renderer.

### 3. "Implement my proposal"

Found [AVALANCHE_SIM_PLAN.md](AVALANCHE_SIM_PLAN.md) already in the repo — a detailed
phased execution plan derived from the proposal. Followed it (it names the node
`MpmSolverNode`).

**Added**

| File | Purpose |
|---|---|
| `webgpu/compute/nodes/MpmSolverNode.{h,cpp}` | Solver node; owns particles/grid/state on GPU |
| `webgpu/compute/shaders/mpm_common.wgsl` | Shared bindings, terrain sampling, signed 3×3 SVD, Stomakhin model |
| `webgpu/compute/shaders/mpm_{prepare,seed,clear_grid,p2g,grid_update,g2p,splat,rasterize}.wgsl` | The eight kernels |
| `apps/webgpu_app/compute/nodes/MpmSolverNodeRenderer.{h,cpp}` | Settings panel + Play/Step/Reset |
| `apps/webgpu_app/resources/graphs/mpm_avalanche_simulation.json` | Ready-to-run example graph |

**Modified**: `webgpu/compute/CMakeLists.txt`, `webgpu/compute/NodeRegistry.cpp`,
`apps/webgpu_app/CMakeLists.txt`, `apps/webgpu_app/compute/nodes/NodeRendererFactory.cpp`,
`apps/webgpu_app/resources.qrc`, `apps/webgpu_app/compute/NodeGraphPanel.cpp` (preset list
entry "Avalanche simulation (MLS-MPM)").

**Key design decisions** (these resolve the open questions in plan §3)

1. **Particle mass normalised to 1**, volume set to `1/density`. Scaling mass and volume by
   the same factor leaves the MPM equations invariant, so this is a pure unit change — but
   it bounds the fixed-point grid accumulators independently of real snow mass, which is
   what makes a compile-time `FIXED_SCALE = 1e4` safe. Without it, avalanche-scale masses
   (~10^6 kg/node) would overflow an `i32`.
2. **One bind group layout for all eight kernels.** Every kernel includes `mpm_common.wgsl`
   and therefore declares an identical binding set, so a whole run — all substeps, splat,
   rasterize — fits in a *single compute pass*. Dispatches within one pass are ordered and
   see each other's storage writes, which is exactly the MPM dependency chain. Grid reset
   is a compute kernel rather than `clearBuffer` specifically so the pass never has to end.
3. **Vertical grid origin derived on-GPU.** A `prepare` kernel scans terrain in the domain
   footprint and `atomicMin`s the altitude into a state buffer; all later kernels read it.
   Avoids an async readback and avoids making the user guess a base altitude.
4. **Collision at both levels** — grid-node Coulomb friction plus a particle position
   clamp, since the node-level condition alone lets snow creep through between nodes.
5. Height sampled with **manual bilinear** — the stitched DEM is R32Float and unfilterable,
   so `textureSample` is unavailable.

**Verification performed**

- `tint` (from vendored Dawn) validates all 8 kernels.
- SVD ported verbatim to Python and checked against numpy over 700+ matrices: exact
  reconstruction, orthonormal U/V with det +1, correct on inverted (det F < 0) and
  near-degenerate inputs.
- Full substep loop ported offline: mass conserved exactly; free fall reproduces `g·t` to
  three decimals; snow lands, settles to rest (v 5.98 → 0.06 m/s), rests exactly on the
  floor; mean `jp` 1.0 → 0.746, i.e. plastic compaction on impact rather than an elastic
  bounce — the Stomakhin behaviour the proposal wanted.
- Booted the app with the MPM graph as startup preset and confirmed via a temporary
  `qInfo` that all 8 shaders compiled and all 8 pipelines were created on Metal with no
  Dawn validation errors. Both temporary changes reverted afterwards.

Incidental finding: `qDebug()` output is filtered in this app's logger; use `qInfo()` for
diagnostics that need to appear in the console.

**Explicitly NOT done**

- **Never run against real terrain.** All verification above covers the math and the
  plumbing, not the terrain coupling. Plan Phase 3's flat-synthetic-field check and Phase 4's
  real-DEM switch still need a human at the keyboard.
- **Phase 5 proper (3D particle rendering) not implemented.** Output is a top-down density
  raster through the existing overlay path (no engine changes). A real `AvalancheRenderer`
  in `webgpu/engine/` remains the right next step; the node already exposes a raw
  `particle buffer` socket so it can bind without CPU readback.
- Parameter tuning at avalanche scale — defaults are Stomakhin's paper values, authored for
  metre-scale snow, not a 1 km domain.

### 4. "export this conversation in a markdown file claude_log.md and keep that updated"

Created this file and recorded the standing instruction in Claude's project memory so
future sessions append to it.

### 5. "How can i test it now" / "How do i know in which region the avalanche is"

Two usability gaps surfaced while writing test instructions, both fixed:

- **No feedback when nothing seeds.** If the domain does not overlap a release area, every
  particle stays inactive and the overlay is simply blank — with no error. Added a
  **"Seed anywhere (ignore release areas)"** debug toggle that fills the whole domain, so a
  first run can confirm the solver works and show where the domain sits. Implemented by
  repurposing a spare `_pad0` float in the uniform, so the struct layout is unchanged.
- **No way to tell where the simulation is.** The region is not configured in the MPM node
  at all — it comes from the **GPX Input** node (`:/gpx/breite_ries.gpx`) via Select Tiles.
  Added a lat/lon readout of the domain centre to the settings panel plus a `qInfo` line on
  every reset (both via `nucleus::srs::world_to_lat_long`).

Location of the default test scenario, computed from the GPX file: **Breite Ries gully,
Schneeberg, Lower Austria**. Track lat 47.77442–47.77892, lon 15.80953–15.82461, elevation
1364–1931 m. Region = 2×2 zoom-15 tiles = 2446 × 2446 m. Default domain (centre 0.5/0.5,
1024 m) is centred at **47.77625, 15.82031**; the track centre sits at normalised
x = 0.352, y = 0.528, i.e. inside the default domain near its western edge — so the default
already covers the gully.

Also corrected misleading UI text: the settings panel only renders for the *selected* node,
and that is what drives stepping, so "keep this panel open" became "keep this node
selected".

### 6. SIGSEGV on toggling "Seed anywhere" — fixed

Crash reported immediately on ticking the new checkbox. Stack:

```
MpmSolverNodeRenderer::render_settings_content()
  -> Node::rerun() -> Node::run() -> MpmSolverNode::run_impl()
    -> update_gpu_settings() -> TextureWithSampler::texture()   <- null deref
```

**Root cause (my bug).** `run_impl()` validated inputs with `is_socket_connected()` only,
then dereferenced the socket data. But *connected is not the same as ready*:
`HeightDecodeNode`'s output socket returns `m_output_texture.get()`, which is **null until
that node has actually run**. `rerun()` re-runs only this node using the last buffered
context, so any UI control that calls it before a full graph run dereferences null.

This was latent in every transport control (Play/Step/Reset) — the checkbox just happened
to be the first one pressed before `Shift+R`.

**Fix**, in two layers:
1. `MpmSolverNode::has_valid_inputs()` checks connectivity *and* non-null payloads;
   `run_impl()` calls `fail_run()` with an actionable message instead of dereferencing.
   This makes the node safe regardless of who calls `rerun()`.
2. The renderer computes `ready` once per frame, wraps the transport buttons in
   `BeginDisabled`, clears `m_playing`, shows "Run the full graph once (Shift+R)", and gates
   the `rerun()` call — otherwise auto-play would spam error modals.

**Lesson for future node work in this repo:** an output socket returning `unique_ptr::get()`
is null before its node runs. Any code path that can trigger a single node out of graph
order must null-check payloads, not just check `is_socket_connected()`.

### 7. Visible, but only animating with the graph editor open — sidebar panel added

Two reports after the first successful run (9.36 s simulated, domain centre correct):

**a) Nothing visible on screen.** Not a solver bug — a display one. The domain was 1024 m
across a 1024-texel raster (1 m/texel) and `mpm_splat` wrote each particle into *exactly
one* texel. 65536 particles over 1,048,576 texels covered ~6% at count 1, i.e. isolated
1-metre pixels at ~37% alpha seen from kilometres away. Rendering was correct; what it was
told to render was invisible. Particles represent parcels of snow, not points.

Fixes: `mpm_splat` now draws a disc of `splat_radius` metres (default 6 m, loop capped at 8
texels); `mpm_rasterize` normalises against a CPU-computed `density_reference` (the coverage
a uniform spread would give) instead of a magic `/6.0`, so the display rescales itself when
particle count or resolution change; default raster 1024 -> 512. Uniform struct grew to
144 B. New "Splat radius" slider plus an "Output texel: X m" readout.

**b) Animation only advanced while the graph editor was open.** Root cause: stepping was
driven from `MpmSolverNodeRenderer::render_settings_content()`, which only runs while the
node graph editor is open *and* that node is selected.

Fix: new `apps/webgpu_app/avalanche/AvalanchePanel.{h,cpp}`, a sidebar panel that owns the
transport. Key detail of the `ImGuiPanel` interface: `draw_panel()` renders inside the
sidebar (so it stops when the section is collapsed), while **`draw()` runs every frame
regardless of panel visibility**. Stepping therefore lives in `draw()`, and the animation
keeps running with the sidebar section collapsed and the editor closed.

The panel resolves the solver by scanning `NodeGraphPanel::node_graph()` (new accessor) with
a `dynamic_cast` every frame, because loading a preset replaces the graph and every node in
it. It renders nothing when the active graph has no MPM node, so it only appears for the
MLS-MPM preset.

Play/Pause was *removed* from the node renderer so there is exactly one animation driver;
Step and Reset are one-shot and stayed. Registered under `ALP_WEBGPU_APP_ENABLE_COMPUTE`
right after `NodeGraphPanel`, whose pointer it holds.

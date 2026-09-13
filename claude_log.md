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

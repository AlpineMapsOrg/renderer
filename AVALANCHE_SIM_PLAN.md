# Avalanche Simulation & Visualisation — Working Plan

Personal working plan for the VisComp WS26 project. Source of truth for scope/rationale is the Notion proposal and its subpages (linked at the bottom); this file is the execution plan derived from them, plus what a repo walkthrough on 2026-08-25 actually found in this fork. Update it as decisions get made — treat it as living, not a one-time export.

**Non-goal of this document:** no code. It tells you what to build, in what order, where it goes, and what to figure out before you write it.

## 1. What we're building (one paragraph)

A physically based MLS-MPM snow/avalanche solver that runs as WebGPU compute passes inside weBIGeo, coupled to the real terrain the app already renders, plus a particle-based renderer for the result. It sits **alongside** the avalanche simulation weBIGeo already has (a cheap statistical trajectory/runout model), not instead of it — reusing the same terrain-prep front end. Minimal deliverable: one stored scenario, MLS-MPM particles falling and flowing over real terrain, rendered as points/billboards, at interactive frame rates on desktop. Stretch: CK-MPM kernel swap, better rendering, avalanche.report-driven scenarios, upstream PR.

## 2. Repo orientation (as found, not as assumed)

This matters because the Notion research subpages were written partly from GitHub browsing / AI research and explicitly flagged some of this as unconfirmed. Checked directly in the connected `webigeo` checkout:

- Remotes: `origin` = `weBIGeo/webigeo` (your fork), `upstream` = `AlpineMapsOrg/renderer`. Currently on `main`, and `main` already contains a recent merge from upstream — so the "which repo to target" open question in the research doc is resolved: **work against `origin/main`**, it's not stale.
- Other origin branches exist that are relevant to skim before you design anything: `release/netidee6745` (matches the published weBIGeo paper), `release/lawinensymposium2025`, `refactor/node-graph`, `feature/eval`. Worth a quick look, not a blocker.
- Module layout actually on disk (differs slightly from the README's mermaid diagram, which still shows `webgpu_engine`/`webgpu_compute` as separate top-level dirs — they've since been merged under `webgpu/`):
  - `nucleus/` — tile scheduler/cache, camera, shared data structures. Also already has `nucleus/avalanche/` (report loading / EAWS danger-scale stuff — relevant for the avalanche.report stretch goal, not for the solver).
  - `webgpu/base/` — RAII WebGPU wrappers, WGSL preprocessor, resource registry (`docs/webgpu_base.md`).
  - `webgpu/compute/` — the compute node graph: `NodeGraph`, `NodeRegistry`, `nodes/*`, `shaders/*.wgsl`.
  - `webgpu/engine/` — the renderer: `Context`, `Window`, per-feature renderers (`tile_mesh/`, `cloud/`, `atmosphere/`, `track/`), `overlay/` (`docs/webgpu_engine.md`).
  - `apps/webgpu_app/` — the application: `App`/`RenderingContext`/`ImGuiManager`, `compute/` (NodeGraphPanel + per-node UI renderers), `overlay/` (per-overlay UI), `ui/` (general panels), `resources/graphs/*.json` (saved node graphs).
  - `gl_engine/`, `app/` — AlpineMaps.org's OpenGL renderer. Out of scope per the proposal; don't touch.
- **Docs to read before touching anything**: `docs/webgpu_app_dev.md` (app structure, the exact 3-file recipe for adding a compute node), `docs/webgpu_engine.md` (render pass order, what a "Renderer" vs an "Overlay" is), `docs/webgpu_base.md` (RAII/shader plumbing). These are short and answer most "how does X work" questions faster than reading source.

### 2.1 The existing avalanche pipeline (confirmed in code, not just the paper)

Terrain-prep chain, all in `webgpu/compute/nodes/`: `SelectTilesNode` → `RequestTilesNode` → `TileStitchNode` (stitches DEM tiles into one height texture, capped at 8192×8192, ~1 m/texel — this is the "stable" terrain your solver should collide against, *not* the render-side streaming tiles) → `HeightDecodeNode` → `ComputeNormalsNode` → `ComputeReleasePointsNode` (slope-threshold release areas, params `min_slope_angle`/`max_slope_angle`/`sampling_interval`, defaults 30°–45°). Then the existing `ComputeAvalancheTrajectoriesNode` (large, ~24 KB — the Monte-Carlo GMF/runout model, multiple friction models, this is the thing being complemented, not replaced) produces the current 2D overlay. Saved example graphs: `apps/webgpu_app/resources/graphs/avalanche_simulation.json` and `avalanche_simulation_with_exports.json` — open these in the node graph editor early, before writing anything, to see the wiring live.

### 2.2 `IterativeSimulationNode` — read this one closely first

`webgpu/compute/nodes/IterativeSimulationNode.{h,cpp}` + `webgpu/compute/shaders/iterative_simulation_compute.wgsl`, graph: `apps/webgpu_app/resources/graphs/iterative_simulation_wip.json` (marked WIP). It already takes a **height texture** and a **release point texture** as inputs and runs a bounded per-iteration compute loop with ping-pong buffers (`m_input_parent_buffer`/`m_output_parent_buffer`, a flux buffer, ping-ponged textures) via a `CombinedComputePipeline`. It is not MPM (looks like a cellular/flux-style spreading model, and it's unfinished), but it is the closest existing analogue to "a node that owns GPU state and iterates it across multiple compute dispatches with the same terrain inputs your solver needs." Read it end to end before designing `MpmSolverNode` — it will save you from re-deriving the ping-pong / bind-group-layout / pipeline-registration boilerplate from scratch, and its shader is a second worked example of a WGSL compute kernel next to `avalanche_trajectories_compute.wgsl`.

### 2.3 Node-graph mechanics you need before writing `MpmSolverNode`

From `Node.h` + `docs/webgpu_app_dev.md`:
- A node declares typed `InputSocket`/`OutputSocket` lists in its constructor; `Data` is a fixed `std::variant` of allowed payload types (tile id lists, `TileStorageTexture*`, `RawBuffer<uint32_t>*`, `TextureWithSampler*`, AABBs, `glm::uvec2`). If particle/grid buffers need a new payload type, that variant is the one place to extend.
- `run_impl()` is the only thing you override; call `complete_run()`/`fail_run()`. The base class handles queuing/re-entrancy.
- Adding a node type touches exactly three places: the `Node` subclass in `webgpu/compute/nodes/`, one line in `NodeRegistry::NodeRegistry()`, and — optionally, for a settings panel in the editor — a `NodeRenderer` subclass in `apps/webgpu_app/compute/nodes/` plus a branch in `NodeRendererFactory::create()`.
- Renderer vs. Overlay distinction matters for the visualisation half: an `Overlay` (`webgpu/engine/overlay/`) is screen-space-only and reads/writes the colour+depth buffer — the proposal itself already rules this out for particles ("cannot be expressed as a terrain overlay"). What we need is a new **Renderer** (like `TileMeshRenderer`/`CloudRenderer`), registered on `webgpu_engine::Context`, instantiated in `RenderingContext::initialize()`, and called from `Window::paint()` in the right spot in the fixed render sequence (`Atmosphere → TileMesh → Cloud → Overlay → Compose`) so it can depth-test against the terrain that `TileMeshRenderer` already wrote.

## 3. Decisions to make explicitly before/at the start of coding

Carried over from the Notion research page's open questions, trimmed to what's actually blocking:

1. **Buffer layout for particles/grid.** SoA in a `RawBuffer<uint32_t>`-style storage buffer (bit-cast floats), mind `vec3`→16-byte padding (WGSL alignment). Decide the struct layout on paper first — copy the pattern in `ComputeAvalancheTrajectoriesNode`'s `*SettingsUniform` structs and their 4/8-byte alignment comments, they already document this pitfall for this codebase.
2. **Atomics for P2G.** WGSL has no atomic float add — fixed-point `atomic<i32>` accumulation is the standard workaround (per the research doc). Pick a scale factor and validate it against expected mass/momentum magnitudes before writing the real kernel — do this on paper/spreadsheet, not by staring at rendering artifacts later.
3. **Height sampling strategy.** Sample the stitched `TileStitchNode` texture directly each substep, or copy the simulation-domain subset into a storage buffer once at setup (cheaper random access, resample to grid spacing). The research doc leans toward the storage-buffer copy once grid spacing ≠ ~1 m DEM spacing. Decide once you know your target grid spacing.
4. **Where collision is resolved.** Grid-level boundary condition (cheap, stable, quantised) vs. particle-level position clamp (sharper, needs care) vs. both. Default to "both", per the research doc, unless it proves unnecessary.
5. **Fixed inner loop vs. app-driven substeps.** MPM needs a CFL-bounded timestep, decoupled from frame rate. Decide the substep-count knob early since it affects the node's settings struct and the UI panel.

None of these need to be fully answered before Phase 0/1 below — but they should be answered on paper before Phase 3 (writing the actual P2G/grid-update/G2P/advection kernels), because they shape the buffer layout every kernel touches.

## 4. Working conventions

- Native build first (much faster iteration than WASM/emscripten). WASM only once something works natively.
- Follow the existing code style exactly (`.clang-format` is provided): `CamelCase` classes, `snake_case` methods/variables, `m_` member prefix, folder structure mirrors namespace. New files need the GPL-3.0 header (copy the template from any existing file, e.g. `Node.h`).
- New feature branch off `origin/main`, e.g. `feature/mpm-avalanche-simulation`. Don't touch `gl_engine`/`app`.
- Every new node gets registered in `NodeRegistry` immediately, even before its `run_impl()` does anything real — that's what lets you build and test the plumbing (sockets, UI panel, serialization) independently of the physics.

## 5. Phased task breakdown

### Phase 0 — Environment & orientation (get to a running build)
- [ ] Set up the native build (Qt 6.10.1, CMake+Ninja, MSVC2022 on Windows — see `docs/webgpu_app.md` for the exact preset and troubleshooting steps). Confirm `webgpu_app_msvc_debug` builds and runs against the live terrain.
- [ ] Load `apps/webgpu_app/resources/graphs/avalanche_simulation.json` in the running app's node-graph editor, run it, watch the existing overlay render. This is the single fastest way to internalize the node graph model.
- [ ] Load `iterative_simulation_wip.json` too and step through what it does.
- [ ] Read `docs/webgpu_app_dev.md`, `docs/webgpu_engine.md`, `docs/webgpu_base.md` fully (short, high signal).
- [ ] Read `Node.h`, `IterativeSimulationNode.{h,cpp}`, `ComputeReleasePointsNode.h`, `TileStitchNode.h` in the editor with the running app open side by side.

### Phase 1 — Warm-up: a no-op node, end to end
Goal: touch every seam (compute node → registry → UI renderer → graph JSON → build) before any MPM math exists, so that when Phase 3 breaks, you know it's the physics and not the plumbing.
- [ ] Create `MpmSolverNode` (empty `run_impl()` for now) in `webgpu/compute/nodes/`, with the two inputs it will eventually need (`height texture`, `release point texture` — same types as `IterativeSimulationNode`'s sockets) and a placeholder output (e.g. a fixed-size dummy buffer).
- [ ] Register it in `NodeRegistry`.
- [ ] Wire it into a copy of the existing avalanche graph in the node editor, save as a new `.json` under `resources/graphs/`, confirm it runs (no-op) without errors.
- [ ] Optional: minimal `NodeRenderer` subclass so it has a name/socket display in the editor.

### Phase 2 — De-risk the algorithm offline, outside WebGPU/WGSL
Don't debug MLS-MPM math and WGSL/atomics/alignment issues at the same time.
- [ ] Implement (or adapt an existing reference/toy implementation of) 2D or 3D MLS-MPM snow in a throwaway environment you're fast in (e.g. a small Python/NumPy or Taichi script, or a standalone C++ CLI tool) — no terrain, no rendering, just particles falling into a box with gravity and a flat/simple sloped floor. Validate against Stomakhin et al. [5] qualitatively (does it look like snow: plasticity, some cohesion, doesn't behave like pure fluid).
- [ ] Nail down the four-stage loop and the exact per-particle/per-grid-node data you need at each stage (this becomes your buffer layout from decision #1 above).
- [ ] This offline prototype is also useful later as a written appendix in the report ("verified the constitutive model in isolation before the GPU port").

### Phase 3 — Port the solver into `MpmSolverNode`, one stage at a time
- [ ] Particle initialisation: seed particles inside `ComputeReleasePointsNode`'s output area, on/above the stitched terrain height. Get this rendering as raw points (ties into Phase 5 early, as a debug view) before writing any of P2G/grid update/G2P.
- [ ] Grid reset + P2G compute shader (mass/momentum scatter, fixed-point atomics per decision #2).
- [ ] Grid update compute shader (normalize momentum→velocity, gravity, snow constitutive model — start with something simple and get it visibly working, then bring in Stomakhin-style plasticity/hardening).
- [ ] G2P compute shader.
- [ ] Advection compute shader.
- [ ] Wire the four shaders into the per-frame/per-substep dispatch sequence inside `run_impl()`, following `IterativeSimulationNode`'s ping-pong pattern. Expose particle count, grid resolution, and substep count as node settings from the start (mirrors how every other node here exposes tunables via a `*Settings`/`*SettingsUniform` struct pair).
- [ ] Sanity-check on a **flat** synthetic height field before switching to real terrain — isolates solver bugs from terrain-coupling bugs.

### Phase 4 — Terrain coupling
- [ ] Bind the real stitched height texture (decision #3) and implement `terrain_height`/`terrain_normal` sampling in WGSL (manual bilinear — height formats aren't filterable, `textureSample` won't work).
- [ ] Implement collision (decision #4): grid-level boundary condition first, add particle-level clamp if snow visibly clips through terrain.
- [ ] Switch from the flat synthetic test field to the real DEM for one real location.

### Phase 5 — Minimal rendering: direct particle rendering
Per the rendering-techniques subpage, this is explicitly the recommended starting point — simple, robust, doubles as the debug view you'll want throughout Phase 3–4 anyway, so pull pieces of it forward if it unblocks debugging sooner.
- [ ] New `AvalancheRenderer` in `webgpu/engine/` (not an `Overlay` — see §2.3), registered on `webgpu_engine::Context`, instantiated in `RenderingContext::initialize()`, invoked from `Window::paint()` after the geometry pass so it can depth-test against `TileMeshRenderer`'s output.
- [ ] Render particles as points/billboards, bind the solver's particle buffer directly as input (no CPU readback).
- [ ] Colour by an attribute (velocity or height to start — matches the research doc's recommendation and is useful for debugging the solver itself).

### Phase 6 — UI, scenarios, parameter tuning
- [ ] `AvalancheWindow`-style ImGui panel (`apps/webgpu_app/`, follow `NodeGraphPanel`/existing panel conventions) exposing start/pause/reset and the solver settings.
- [ ] A small set of stored scenarios (region + release area + material preset), matching the proposal's "pre-programmed list of stored avalanches."
- [ ] Tune snow material parameters on a best-effort basis against Stomakhin et al. [5]; document what you changed and why (this is explicitly called out as experimental/best-effort in the proposal, not a hard requirement to get "realistic").
- [ ] Quality knobs (particle count, grid resolution, substeps) validated on both a desktop and a lower-end/notebook target if available.

### Phase 7 — Stretch goals (only after Phase 6 works end to end)
- [ ] CK-MPM as a swappable transfer kernel (touches only the P2G/G2P shaders + grid allocation, per the MLS-MPM subpage) — implement behind a settings switch so both kernels are A/B-comparable.
- [ ] Hybrid/upgraded rendering (surface reconstruction for the dense core, per the rendering-techniques subpage) — only if the minimal particle renderer is solid and there's time left.
- [ ] avalanche.report integration (`nucleus/avalanche/` already has report-loading scaffolding — check `ReportLoadService`/`eaws.{h,cpp}` before building a new `AvalancheForecastService` from scratch, it may already do part of this) — CAAML v6 JSON fetch, region polygon join, danger-level → simulation-preset mapping, per the data-integration subpage.
- [ ] Contribution back upstream: coordinate with maintainers on Discord before opening a PR against `AlpineMapsOrg/renderer`.

## 6. Where to actually start this week

1. Get the native build running and load the two existing graphs (`avalanche_simulation.json`, `iterative_simulation_wip.json`) in the editor. This alone will answer half of the "how does the node graph really work" questions faster than reading code.
2. Read `IterativeSimulationNode.{h,cpp}` + its `.wgsl` shader top to bottom — it's the single best template for the plumbing `MpmSolverNode` needs.
3. Do the Phase 1 warm-up node (empty `MpmSolverNode`, registered, wired into a saved graph, builds and runs as a no-op). Small, mechanical, and de-risks the "three files to touch" recipe before physics is in the mix.
4. In parallel (doesn't block 1–3), start the offline MLS-MPM prototype from Phase 2 in whatever language you're fastest in — that's the long pole and the part with the most "does this even look right" uncertainty.

## 7. Risks (from the proposal's own scope/risks section, kept here as a checklist to revisit)

- [ ] Browser/WebGPU performance budget — particle count & grid resolution must stay adjustable; may need to cap simulation area.
- [ ] Snow parameter tuning is experimental, best-effort — don't block the schedule on "looks perfectly realistic."
- [ ] DEM resolution (5–10 m) is coarser than an ideal MPM grid spacing — decide early whether to interpolate the DEM or accept a coarser collision surface (ties into decision #3).
- [ ] Compute-node-graph API may not be expressive enough for a many-substep iterative solver sitting fully inside one node — if `run_impl()` fighting the graph model becomes a real blocker, the fallback (per the research doc) is letting the solver live beside the graph and only publish results into it. Don't pre-solve this; cross it only if Phase 3 actually hits it.

## References

Full detail lives in Notion — this file intentionally doesn't duplicate it:
- [Proposal - Real-Time Avalanche Simulation and Visualisation in weBIGeo](https://app.notion.com/p/3bf39f665f6680f1a9fce1ae4e626853) (main proposal, background, references [1]–[10])
- [Implementation Research - weBIGeo Code Base and MLS-MPM Integration](https://app.notion.com/p/a2e872a0a4b54f3aba14219eb681512d)
- [MLS-MPM and Optional CK-MPM Implementation in weBIGeo](https://app.notion.com/p/d8fdf5f838c3470ba4680cc0f309cb8b)
- [Possible Rendering Techniques for Avalanche Simulation](https://app.notion.com/p/6ad04d3c851341d1959a52910e688a8f)
- [Avalanche.report Data Integration Research](https://app.notion.com/p/1c6b2541d98844afbea747f9209f55ef)

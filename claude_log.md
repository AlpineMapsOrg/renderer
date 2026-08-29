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

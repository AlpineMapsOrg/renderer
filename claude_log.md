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

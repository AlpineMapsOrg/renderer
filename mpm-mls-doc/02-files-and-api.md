# Files and API

## Added

### Solver (engine side, `webgpu/compute/`)

| File | Purpose |
|---|---|
| `nodes/MpmSolverNode.h/.cpp` | The solver. Owns particles, grid and sim state on the GPU; runs the substep loop. |
| `nodes/GeoRegionNode.h/.cpp` | Emits a region AABB from lat/lon + extent, so a scenario needs no GPX file. |
| `shaders/mpm_common.wgsl` | Shared bindings, structs, terrain sampling, B-spline kernel, 3×3 SVD, snow model, collision. Included by every kernel. |
| `shaders/mpm_prepare.wgsl` | Scans terrain for the grid's vertical origin. |
| `shaders/mpm_seed.wgsl` | Places particles in the release disc. |
| `shaders/mpm_clear_grid.wgsl` | Zeroes the grid each substep. |
| `shaders/mpm_p2g.wgsl` | Stage 1: particle → grid. |
| `shaders/mpm_grid_update.wgsl` | Stage 2: momentum → velocity, gravity, collisions. |
| `shaders/mpm_g2p.wgsl` | Stages 3+4: grid → particle, plasticity, advection. |
| `shaders/mpm_splat.wgsl` | Accumulates particles into a density raster. |
| `shaders/mpm_rasterize.wgsl` | Density raster → RGBA texture. |

### App side (`apps/webgpu_app/`)

| File | Purpose |
|---|---|
| `avalanche/AvalanchePanel.h/.cpp` | Sidebar panel: location picker + transport + main knobs. **Owns the animation driver.** |
| `compute/nodes/MpmSolverNodeRenderer.h/.cpp` | Node-editor settings panel (full parameter set). |
| `resources/graphs/mpm_avalanche_simulation.json` | Ready-to-run graph preset. |

## Modified

| File | Change |
|---|---|
| `webgpu/compute/CMakeLists.txt` | Source + shader resource entries. |
| `webgpu/compute/NodeRegistry.cpp` | `register_node()` for `MpmSolverNode`, `GeoRegionNode`. |
| `apps/webgpu_app/CMakeLists.txt` | Source entries. |
| `apps/webgpu_app/compute/nodes/NodeRendererFactory.cpp` | `dynamic_cast` branch for the solver renderer. |
| `apps/webgpu_app/compute/NodeGraphPanel.h` | Added `node_graph()` accessor so other panels can reach the graph. |
| `apps/webgpu_app/compute/NodeGraphPanel.cpp` | Preset list entry. |
| `apps/webgpu_app/ImGuiManager.cpp` | Registers `AvalanchePanel` (guarded by `ALP_WEBGPU_APP_ENABLE_COMPUTE`). |
| `apps/webgpu_app/resources.qrc` | Graph resource. |

**Adding a node touches exactly three places** (per `docs/webgpu_app_dev.md`): the `Node`
subclass, one line in `NodeRegistry`, and optionally a `NodeRenderer` + factory branch.

## `MpmSolverNode`

Namespace `webgpu_compute::nodes`. Subclass of `Node`.

### Sockets

**In:** `region aabb` (`Aabb<2,double>*`), `height texture`, `release point texture`
(`TextureWithSampler*`)

**Out:** `texture` (RGBA result), `domain aabb` (wire to the overlay's region input —
the result covers the *domain*, not the whole region), `density buffer` (`RawBuffer<u32>*`),
`raster dimensions` (`uvec2`), `particle buffer` (`RawBuffer<u32>*` — **this is the hook for
a future 3D particle renderer**, no CPU readback needed).

### Methods

| Method | Notes |
|---|---|
| `run_impl()` | One node execution: optional reset, then `substeps_per_run` MPM steps, then splat + rasterize. Single compute pass. |
| `has_valid_inputs()` | Checks sockets are connected **and** payloads non-null. Must be called before driving the node out of graph order — see the gotcha below. |
| `request_reset()` | Re-scan terrain and reseed on next run. |
| `simulated_time()` | Seconds accumulated since last reset. |
| `domain_aabb()` | World bounds of the simulated box. Only valid after the first run. |
| `set_settings()` / `get_settings()` | Settings are consumed lazily in `run_impl()`, so applying them any time is safe. |
| `serialize_settings()` / `deserialize_settings()` | Graph JSON persistence. |

Private helpers: `ensure_resources()` (reallocates buffers when sizes change; forces a
reset), `create_bind_group()` (rebuilt every run — input textures can be recreated
upstream), `update_gpu_settings()` (fills the uniform; converts lat/lon → region-relative
metres), `write_initial_state()` (seeds the SimState atomics from the CPU).

> **Gotcha, learned the hard way (SIGSEGV).** An output socket returning `unique_ptr::get()`
> is **null until its node has run**. `is_socket_connected()` returning true does *not* mean
> there is data. `rerun()` re-runs only one node using the last buffered context, so any UI
> control that calls it before a full graph run will dereference null. Always gate on
> `has_valid_inputs()`.

## `GeoRegionNode`

No inputs; outputs `region` (`Aabb<3,double>*`). Settings: centre lat/lon, extent (m),
min/max altitude.

Corrects for Web Mercator not being equal-area — one projected metre is `1/cos(latitude)`
real metres, so the requested extent is scaled by that. Without it the same "4000 m" gives
noticeably different ground coverage at different latitudes.

Replaces `GPXTrackNode` as the region source in the MPM preset. `SelectTilesNode` takes a
plain AABB, so nothing else in the chain cares.

## `AvalanchePanel`

Sidebar panel, registered after `NodeGraphPanel` (whose pointer it holds).

**The important design point:** `ImGuiPanel::draw_panel()` renders *inside* the sidebar and
stops when the section is collapsed; **`draw()` runs every frame regardless of panel
visibility**. The stepping lives in `draw()`, so the animation keeps running with the
sidebar collapsed and the node editor closed. Driving it from the node renderer (the first
attempt) meant the animation only advanced while that editor was open and the node selected.

`find_node<T>()` re-resolves nodes by `dynamic_cast` **every frame** — loading a preset
replaces the graph and every node in it, so a cached pointer would dangle.

`apply_scenario()` writes region/zoom/domain/release into the nodes, forces a reset, pauses
playback (terrain must be fetched first) and calls `graph->run()`.

Play/Pause deliberately exists **only here** — two things calling `rerun()` per frame would
race. The node renderer keeps Step and Reset, which are one-shot and safe.

## Where the settings live

Split by how often you touch them:

- **Sidebar** (`AvalanchePanel`): location, transport, substeps, dt, splat radius, domain
  lat/lon + size + grid resolution, release lat/lon + radius, seed-anywhere.
- **Node editor** (`MpmSolverNodeRenderer`): all of the above plus the snow material set
  (E, ν, ξ, θ_c, θ_s), particle count, densities, slab thickness, CFL readout.

# Files and API

## Added

### Solver (engine side, `webgpu/compute/`)

| File | Purpose |
|---|---|
| `nodes/MpmSolverNode.h/.cpp` | The solver. Owns particles, grid and sim state on the GPU; runs the substep loop. |
| `nodes/GeoRegionNode.h/.cpp` | Emits a region AABB from lat/lon + extent, so a scenario needs no GPX file. |
| `TimeSteppedSimulation.h` | Interface of a simulation the driver plays back frame by frame; `MpmSolverNode` implements it. See [13-simulation-driver.md](13-simulation-driver.md). |
| `SimulationClock.h/.cpp` | Per-simulation playback: real time (fixed-step accumulator, speed 0.1–4x) or accuracy (as fast as the GPU budget allows); play/pause/step and the speed readout. |
| `SimulationDriver.h/.cpp` | Ticks the clock of every `TimeSteppedSimulation` in the graph once per frame, from `ImGuiManager::render()`. |
| `nodes/AvaFrameSandboxNode.h/.cpp` | AvaFrame sandbox: loads a dataset prepared by `mpm-mls-doc/avaframe/prepare_dataset.py` or the panel's download (`AvaFrameData`) (DEM, release, com1DFA footprint) and feeds the solver in metres. Defines `SandboxTerrain`. |
| `shaders/mpm_common.wgsl` | Shared bindings, structs, terrain sampling, B-spline kernel, 3×3 SVD, matrix helpers. Included by every kernel. |
| `shaders/mpm_material.wgsl` | Constitutive-model dispatcher: `switch` on `settings.constitutive_model`. |
| `shaders/mpm_material_stomakhin.wgsl` | Stomakhin 2013: fixed corotated + SV clamp + exponential hardening. |
| `shaders/mpm_material_drucker_prager.wgsl` | Klár 2016: Hencky elasticity + friction cone, closed-form projection. Cohesionless. |
| `shaders/mpm_material_ccc.wgsl` | Gaume 2018: Hencky elasticity + Cam-Clay ellipse, sinh hardening; Wolper 2019 NACC return incl. its shear hardening (§6.2.3). |
| `shaders/mpm_friction.wgsl` | Basal-friction dispatcher: `switch` on `settings.basal_friction_model`; Coulomb, Voellmy (local flow depth from the grid update). |
| `shaders/mpm_prepare.wgsl` | Scans terrain once per reset: altitude range for the readout, and the per-column floor of the terrain-following grid. |
| `shaders/mpm_seed.wgsl` | Places particles in the release disc (map) or by release depth per cell (sandbox). |
| `shaders/mpm_p2g.wgsl` | Stage 1: particle → grid. |
| `shaders/mpm_grid_update.wgsl` | Stage 2: momentum → velocity, gravity, collisions. |
| `shaders/mpm_g2p.wgsl` | Stages 3+4: grid → particle, plasticity, advection; per-substep diagnostics reduced per workgroup. |
| `shaders/mpm_column_stats.wgsl` | Sandbox only: sums particle speeds per grid column for the peak velocity raster. |
| `shaders/mpm_refresh_stress.wgsl` | Recomputes the cached particle stress after a material edit without a reseed. |
| `shaders/mpm_splat.wgsl` | Accumulates particles into a density raster. |
| `shaders/mpm_rasterize.wgsl` | Density raster → RGBA texture. |

### Rendering (`webgpu/engine/`)

The 3D avalanche on the map (map mode only; the sandbox keeps its 2D view). Design, history
and verification: [12-rendering-plan.md](12-rendering-plan.md).

| File | Purpose |
|---|---|
| `avalanche/AvalancheRenderer.h/.cpp` | Draws the particles and the flow surface. `prepare()` (compute: splat + field, before the G-buffer pass), `draw()` (particles + surface into the G-buffer), `draw_transparent()` (Powder, after compose). Fed by a source callback (`Frame`), not by the solver directly. |
| `shaders/render_avalanche.wgsl` | Particles as sphere or strain-rate-ellipsoid impostors in the G-buffer, optionally as 8/27 detail (child) particles. Writes the avalanche marker into the overlay slot. |
| `shaders/render_avalanche_powder.wgsl` | Powder: large soft transparent blobs with per-particle noise, blended over the lit frame, faded against the G-buffer depth. |
| `shaders/avalanche_surface_splat.wgsl` | Flow surface 1/3: B-spline splat of particle volume onto the render grid (fixed-point atomics). |
| `shaders/avalanche_surface_field.wgsl` | Flow surface 2/3: depth, blanket height, speed, and the persistent peak depth (track). |
| `shaders/render_avalanche_surface.wgsl` | Flow surface 3/3: the draped blanket and the track into the G-buffer. |
| `shaders/avalanche_surface_common.wgsl`, `avalanche_surface_terrain.wgsl` | Shared uniform/layouts, B-spline, avalanche marker; the solver's terrain sampled like `mpm_common`. |

### App side (`apps/webgpu_app/`)

| File | Purpose |
|---|---|
| `avalanche/AvalanchePanel.h/.cpp` | Sidebar panel: location picker + transport + playback (mode, speed, pacing) + main knobs, through the solver's clock. Also the sandbox dataset picker. |
| `compute/ParticleRenderNode.h/.cpp` | Terminal node "Particles 3D": hands the solver's `particle view` to the engine's `AvalancheRenderer`, serialises its settings, and owns the "Avalanche snow cover" overlay (`sync_snow_cover()`, snow line fitted to the solver's terrain scan). See [12-rendering-plan.md](12-rendering-plan.md). |
| `avalanche/SandboxView.h/.cpp` | 2D window for sandbox runs: hillshade, release, com1DFA outline, particle raster, domain box. |
| `avalanche/AvaFrameData.h/.cpp` | "Download AvaFrame data" in the panel: downloads the five real test avalanches from AvaFrame's repository (release 2.1) and prepares them. A C++ port of `prepare_dataset.py` for these five, with byte-identical rasters. |
| `compute/nodes/MpmSolverNodeRenderer.h/.cpp` | Node-editor settings panel (full parameter set). |
| `resources/graphs/mpm_avalanche_simulation.json` | Ready-to-run graph preset. |
| `resources/graphs/mpm_avaframe_sandbox.json` | Sandbox preset: `AvaFrameSandboxNode` → `MpmSolverNode`, no map nodes. |

## Modified

| File | Change |
|---|---|
| `webgpu/compute/CMakeLists.txt` | Source + shader resource entries. |
| `webgpu/compute/NodeRegistry.cpp` | `register_node()` for `MpmSolverNode`, `GeoRegionNode`, `AvaFrameSandboxNode`. |
| `webgpu/compute/nodes/Node.h` | `const SandboxTerrain*` and `const MpmParticleView*` (the solver's `particle view` output) added to the socket data variant. |
| `apps/webgpu_app/CMakeLists.txt` | Source entries. |
| `apps/webgpu_app/compute/nodes/NodeRendererFactory.cpp` | `dynamic_cast` branch for the solver renderer. |
| `apps/webgpu_app/compute/NodeGraphPanel.h` | Added `node_graph()` accessor so other panels can reach the graph. |
| `apps/webgpu_app/compute/NodeGraphPanel.cpp` | Preset list entry. |
| `apps/webgpu_app/ImGuiManager.h/.cpp` | Registers `AvalanchePanel` (guarded by `ALP_WEBGPU_APP_ENABLE_COMPUTE`); owns the `SimulationDriver` and ticks it at the start of `render()`, also with the GUI hidden. |
| `unittests/CMakeLists.txt`, `unittests/webgpu_compute/` | `unittests_webgpu_compute`: the simulation clock against a fake simulation. |
| `apps/webgpu_app/resources.qrc` | Graph resource. |
| `webgpu/engine/Context.h/.cpp`, `apps/webgpu_app/RenderingContext.cpp` | The engine context owns an `AvalancheRenderer` (`avalanche_renderer()`); the app creates it and registers `ParticleRenderNode`. |
| `webgpu/engine/Window.cpp` | Calls the renderer: `prepare()` before the G-buffer pass, `draw()` after the tiles, `draw_transparent()` after compose (before the track lines). |
| `webgpu/engine/overlay/Overlay.h`, `OverlayRenderer.cpp` | `Overlay::enabled`: disabled overlays are left out of the ping-pong chain (the 2D density overlay hides while the 3D view is on). |
| `apps/webgpu_app/compute/OverlayRenderNode.h/.cpp` | `set_overlay_enabled()`, used by the Avalanche panel. |
| `webgpu/engine/overlay/ScreenSpaceSnowOverlay.h/.cpp`, `shaders/overlays/screen_space_snow.wgsl`, `apps/webgpu_app/overlay/ScreenSpaceSnowOverlayImGuiRenderer.cpp` | Snow skips pixels carrying the avalanche marker (binds the G-buffer overlay slot); new `patches` setting (slider "Patches"): thin snow as sparse noise patches instead of a translucent veil. |

**Adding a node touches exactly three places** (per `docs/webgpu_app_dev.md`): the `Node`
subclass, one line in `NodeRegistry`, and optionally a `NodeRenderer` + factory branch.

## `MpmSolverNode`

Namespace `webgpu_compute::nodes`. Subclass of `Node`.

### Model selection

Two independent enums, mirrored by `u32` constants in the WGSL dispatchers:

```cpp
enum ConstitutiveModel  : uint32_t { STOMAKHIN_2013 = 0, DRUCKER_PRAGER = 1, COHESIVE_CAM_CLAY = 2 };
enum BasalFrictionModel : uint32_t { COULOMB = 0, VOELLMY = 1 };
```

Set through `MpmSolverSettings::constitutive_model` / `basal_friction_model`, serialised as
ints, switchable at runtime with no pipeline rebuild. Recipe for adding one in
[05-tuning.md](05-tuning.md#adding-a-constitutive-model).

### Sockets

**In:** `region aabb` (`Aabb<2,double>*`), `height texture`, `release point texture`
(`TextureWithSampler*`), and the optional `sandbox` (`const SandboxTerrain*`). Connecting
`sandbox` switches the solver to the AvaFrame sandbox:
- the region is taken as metres (no Mercator factor);
- the domain is centred on the dataset's suggestion, and may use the DEM's longer side;
- the release texture is read as vertical depth per cell (R32Float) instead of a mask on a disc
  (`release_mode = 1`, see 03-shaders `mpm_seed`).

See `mpm-mls-doc/avaframe/README.md`.

**Out:** `texture` (RGBA result), `domain aabb` (wire to the overlay's region input —
the result covers the *domain*, not the whole region), `density buffer` (`RawBuffer<u32>*`),
`raster dimensions` (`uvec2`), `particle buffer` (`RawBuffer<u32>*`), and `particle view`
(`const MpmParticleView*`): what the 3D renderer needs without CPU readback — the buffer
and count, the metres → world affine, particle spacing, the grid (origin, dx, columns), the
solver's height texture and region size, and `reset_count` (changes on Reset; clears the
track). Kept current across reallocations.

### Methods

| Method | Notes |
|---|---|
| `run_impl()` | One node execution: optional reset, then `substeps_per_run` MPM steps (or what `advance()` asked for), then splat + rasterize. Submitted in chunks by `submit_chunk()`. |
| `advance(steps, chunks, submission)`, `submit_frame()` | `TimeSteppedSimulation`: one run with these counts, without writing the settings. `Chained` = the old way (below); `PerFrame` = one chunk now and one per `submit_frame()`, skipped while three are in flight; done once the last one is submitted. See [13-simulation-driver.md](13-simulation-driver.md). |
| `submit_chunk()` | Encodes and submits the chunk's share of the run's substeps (spread evenly, they differ by at most one) as one command buffer (first chunk: raster clear, on reset also grid + tile-flag clear and prepare + seed; last chunk: splat + rasterize + state copy into the readback buffer). In a chained run it re-arms itself from the queue's work-done callback, two chunks in flight, so rendered frames interleave with the run. Chunks carry timestamp writes when their query slot is free, and the run's serial for their callback. |
| `perf_stats()` | `PerfStats`: runs since reset, ms per run, **GPU ms per substep** (timestamp queries; 0 without `timestamp-query`), solver GPU bytes. Read by the simulation clock (`gpu_ms_per_step()`) and the panel's readout. |
| `has_valid_inputs()` | Checks sockets are connected **and** payloads non-null. Must be called before driving the node out of graph order — see the gotcha below. |
| `request_reset()` | Re-scan terrain and reseed on next run. |
| `simulated_time()` | Seconds accumulated since last reset. |
| `last_state()` | `SimStateReadback`: active particles, particles that yielded in the last substep, max speed, terrain range, centre of mass, mean \|v\|², mean particle path, box contacts (band ceiling, walls) with the run's substep count, seeded share of the release disc. Async — describes the *previous* completed run; `valid` false until the first arrives. |
| `energy_line()` | Vector of `EnergySample {time, path, altitude, energy_height}` — `path` the mass-averaged particle path — one per completed run since the last reset; halved when it reaches `MAX_ENERGY_SAMPLES` (2048). |
| `energy_line_friction()` | `−slope` of energy height over horizontal path, fitted over the sliding regime (past 10 % of total path). The effective friction coefficient the flow experiences; NaN until there is movement. |
| `domain_aabb()` | World bounds of the simulated box (sandbox metres in the sandbox). Only valid after the first run. |
| `at_rest()` | com1DFA's stop criterion: mean kinetic energy below `stop_kinetic_fraction` (default 0.01) of its peak since the reset. One run behind, like `last_state()`. The caller decides what to do (the panel pauses in the sandbox). |
| `read_back_peaks(callback)` | `ColumnPeaks`: peak depth and peak mean particle speed per grid column since the reset, with domain origin, dx and simulated time. Own staging buffer per request; the callback fires from the event pump and not at all if the node is gone. |
| `sandbox()` | The connected `SandboxTerrain`, or nullptr on the map. |
| `set_settings()` / `get_settings()` | Settings are consumed lazily in `run_impl()`, so applying them any time is safe. |
| `serialize_settings()` / `deserialize_settings()` | Graph JSON persistence. |

Private helpers: `ensure_resources()` (reallocates buffers when sizes change, clamped to the
device's storage-binding limit; forces a reset), `ensure_bind_group()` (cached; rebuilt when
our buffers, the input texture handles or the graph run id change — upstream nodes recreate
their textures on a full graph run), `update_gpu_settings()` (fills the uniform; converts
lat/lon → region-relative metres), `write_initial_state()` (seeds the SimState atomics from
the CPU), `create_pipelines()` (called through the one pipeline factory registered per
resource registry, for every live solver — see 09-performance-analysis.md §5, L3).

Async callbacks (work done, buffer maps) never capture `this`: they carry a copy of
`m_alive`, a `shared_ptr<MpmSolverNode*>` the destructor sets to null, so a callback that
fires after the graph was replaced returns instead of touching a destroyed node.

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

**The important design point:** the panel does not step the solver. `SimulationDriver`
does, from `ImGuiManager::render()`, every frame and also with the GUI hidden (since
2026-10-24, [13-simulation-driver.md](13-simulation-driver.md)). Before, the stepping lived in
the panel's `draw()`, which runs every frame regardless of panel visibility but not with the
GUI hidden; before that, in the node renderer, which only advanced while that editor was
open and the node selected. The panel's `draw()` still writes the playback settings into the
solver's clock (`sync_clock()`) and runs the sandbox logic: the at-rest stop, the export and
batch mode.

`find_node<T>()` re-resolves nodes by `dynamic_cast` **every frame** — loading a preset
replaces the graph and every node in it, so a cached pointer would dangle.

`apply_scenario()` writes region/zoom/domain/release into the nodes, forces a reset, pauses
playback (terrain must be fetched first) and calls `graph->run()`.

`apply_material_preset()` writes model + E/ν/ρ/μ + model-specific parameters into the
solver settings, forces a reseed (plastic state is model-specific) and pulls `dt` under
the CFL bound of the new stiffness (`MpmSolverNode::max_stable_dt()`, which the node also
enforces on every run). Seven presets: Stomakhin, Li 2021 Cases I–V (named "… parameters":
the notes say what Li observed), and a Drucker–Prager cold-dense alternative. The sidebar
warns when the last run touched the box (band ceiling or walls).

**3D view (map graph, "Particles 3D" node).** Controls under the transport: "3D particles"
(+ colour), "Flow surface", "Look at the flow" (camera 400 m from the centre of mass),
"Track", "Snow cover", "Surface detail", particle shape (spheres / strain-rate ellipsoids /
Powder, the default) with size, detail particles or powder size/opacity, strain window.
`draw()` hides the 2D density overlay while the 3D view is on and calls
`ParticleRenderNode::sync_snow_cover()` with the solver's terrain range every frame.

Play/Pause deliberately exists **only here** (through the clock) — two things starting runs
per frame would race. The node renderer keeps Step and Reset, which are one-shot and safe;
its Step runs `substeps_per_run` from the settings.

**Playback.** "Playback" (Real time / Accuracy), "Speed" (0.1–4x, real time only) and
"Pacing" (Manual / Smooth / Balanced / Fast) under the transport. Mode and pacing are
remembered per context (map: real time + Smooth, sandbox: accuracy + Balanced), the speed
once; batch mode always plays at accuracy, Fast.

**Startup (2026-09-14).** "Load this simulation at launch" (+ "and play") in the panel.
`ready()` runs after `NodeGraphPanel::ready()` has loaded the default graph; with
autostart on it replaces that with the MLS-MPM preset, applies the remembered material
preset and scenario (which runs the graph) and, if asked, starts playing once the solver
has valid inputs. Stored per user with `QSettings("weBIGeo", "avalanche")` — on macOS
`~/Library/Preferences/com.webigeo.avalanche.plist`, keys `startup/autostart`,
`startup/autoplay`, `startup/scenario`, `startup/material_preset` — deliberately *not* in
the graph JSON, so the preset stays a plain preset. When the active graph has no MPM
node, the panel now shows a "Load the MLS-MPM simulation" button instead of hiding.
`NodeGraphPanel::load_preset()` became public for this (one moved line upstream).

*Web build* (`__EMSCRIPTEN__`): no settings store — Qt's WASM `QSettings` backend loads
asynchronously and the deployment exists to show the simulation — so it **loads by
default** and the URL decides: `webgpu_app.html?avalanche=0` skips it, `?avalanche=play`
also starts playing (read once with `EM_ASM_INT` from `window.location.search`).
Temporary for the release; verified in Chrome via a COOP/COEP static server on the
wasm release build.

## Where the settings live

Split by how often you touch them:

- **Sidebar** (`AvalanchePanel`): location, transport, substeps, dt, splat radius, domain
  lat/lon + size + grid resolution, release lat/lon + radius, seed-anywhere.
- **Node editor** (`MpmSolverNodeRenderer`): all of the above plus the snow material set
  (E, ν, ξ, θ_c, θ_s), particle count, densities, slab thickness, CFL readout.

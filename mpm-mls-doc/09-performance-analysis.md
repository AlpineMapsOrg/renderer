# Performance

How fast the MLS-MPM solver is, where its GPU time goes, how much memory it needs, how it shares
the GPU with the map, and what would make it faster. Scope: the solver (`MpmSolverNode`, the
`mpm_*.wgsl` kernels) and its playback; the rest of weBIGeo is taken as given.

All GPU times are from the kernel benchmark ([bench/](bench/README.md), §7) on an Apple M5 in
Chrome 158 with Metal, on its analytic slope, unless stated otherwise. Every result file is
tabulated in [10-benchmark-results.md](10-benchmark-results.md).

## 1. Summary

| Configuration | GPU per run (24 substeps) | per substep | speed at dt 0.01 s | at the preset's dt 0.02 s |
|---|---|---|---|---|
| **Medium** (graph preset): 65 536 particles, 320² × 12 | **6.4 ms** | 0.27 ms | 34× real time | ~70× |
| High: 131 072 particles, 320² × 16 | 12.5 ms | 0.52 ms | 18× | ~36× |
| 1 048 576 particles, 320² × 16 | 106 ms | 4.4 ms | 2.2× | ~4.5× |

"Speed" is the GPU-bound ceiling: simulated seconds per wall second if the solver had the GPU
to itself. In the app the solver gets a fixed slice of each frame (§5), and the map plays the
avalanche in real time, so the ceiling is headroom, not the playback speed.

- **Where the time goes:** P2G and G2P, about 45 % each; the sort and the grid update are
  under 5 % each (§3.1).
- **The material model barely matters:** Stomakhin, Drucker–Prager and Cohesive Cam Clay cost
  within 4 % of each other (§3.2).
- **Memory:** 69 MiB at Medium, 100 MiB at High, well inside any device that runs weBIGeo
  (§4).
- **No leaks:** solver memory is constant over a 15-minute run, process memory does not grow
  (§6).

## 2. The substep on the GPU

```
once per chunk:  sort particle indices by 4 × 4-column tile        (count → scan → scatter)
each substep:    P2G         tile order, sums each tile in workgroup memory, flags active tiles
                 grid update active 8 × 8-column tiles only, zeroes what it read
                 G2P         buffer order, one SVD, advection, diagnostics
```

All of it runs in one compute pass per submitted chunk; [03-shaders.md](03-shaders.md) describes
each kernel. Four choices make it fast:

### 2.1 Grid work follows the snow

The grid is grouped into 8 × 8-column tiles. P2G flags the tiles its stencil touches; the grid
update runs one workgroup per tile and returns at once for an unflagged tile. The grid update
is the last reader of the fixed-point accumulators and zeroes the nodes it read, so there is no
clear pass: nodes outside flagged tiles were never written and stay zero. The snow covers 1 %
of the columns at release and rarely more than 10–20 % during the runout, so the grid update
costs under 5 % of a run, whatever the domain size.

It writes the node velocities as floats (`grid_velocity`), so G2P reads them with plain loads
instead of atomic loads and a fixed-point round trip.

### 2.2 P2G in tile order with workgroup accumulation

The cost of P2G is its 27 × 5 fixed-point atomics per particle: at the preset's resolution
(12.5 m cells, a 1–2 m slab) there are hundreds of particles per cell, so neighbouring particles
hit the same nodes and their atomics serialise. Once per chunk a counting sort orders the
particle indices by 4 × 4-column tile; one P2G workgroup then takes one tile's particles and
sums their contributions in a 6 × 6 × 16-node window in workgroup memory, and flushes the window
to the grid with one atomic per node and accumulator. Particles of another tile (where a tile's
run of particles starts or ends inside the workgroup) and layers above the window's 16 write to
the grid directly.

This makes P2G about 3× faster (§3.1). The sort costs 0.2 ms per run at 131 k particles,
1.4 ms at 1 M. The sort data shares the tile-flag buffer, so the solver stays at eight storage
buffers. The fixed-point sums are order-independent, so the result is bit for bit the same as
in buffer order.

### 2.3 One SVD per particle and substep

Each material's return mapping also returns the Kirchhoff stress τ of the state it returns,
from the SVD it already computed. The particle stores τ in its padding, and P2G reads it. P2G
needs no SVD of its own; the particle stays 128 bytes.

### 2.4 G2P stays in buffer order

G2P reads and writes the whole 128-byte particle. In tile order it was slower (6.1 vs 5.8 ms per
run at 131 k, 66 vs 62 ms at 1 M): scattered particle access costs more than the better grid
locality saves.

## 3. Measurements

### 3.1 By particle count

Cam Clay (Li 2021 case V), dt 0.01 s, 20 measured runs of 24 substeps. GPU ms per run:

| Particles, grid | total | P2G | grid update | G2P | solver memory (benchmark) |
|---|---|---|---|---|---|
| 65 536, 320² × 12 | 6.42 | 2.87 | 0.33 | 3.01 | 61 MiB |
| 131 072, 320² × 16 | 12.49 | 5.59 | 0.51 | 5.96 | 84 MiB |
| 262 144, 320² × 16 | 27.42 | 11.47 | — | — | — |
| 524 288, 320² × 16 | 55.53 | 22.35 | — | — | — |
| 1 048 576, 320² × 16 | 106.07 | 41.44 | 0.46 | 62.04 | 199 MiB |

The cost is linear in the particle count; the grid update does not depend on it. The rest of
a run (sort, splat, rasterise) is a few per cent. The app needs 8 MiB more per 65 536 particles
than the benchmark, for the previous particle state that real-time playback interpolates from
(§4).

Before the sorted P2G, the same runs took 1.8–2.2× longer (P2G 70 % of a run); before the
active tiles and the single SVD, the preset's 131 k particles took about 100 ms per run. The
dated files are in [10-benchmark-results.md](10-benchmark-results.md).

### 3.2 By material model

131 072 particles, 320² × 16, dt 0.01 s:

| Model | GPU per run | P2G / grid update / G2P |
|---|---|---|
| Stomakhin | 12.06 ms | 5.37 / 0.48 / 5.81 |
| Drucker–Prager | 12.06 ms | 5.40 / 0.47 / 5.79 |
| Cohesive Cam Clay | 12.49 ms | 5.59 / 0.51 / 5.96 |

The return mappings differ in cost (Cam Clay's sinh hardening and three-case projection is the
most expensive), but next to the transfers that hardly shows.

### 3.3 Time step

The time step is free speed: simulated time per substep is proportional to `dt`, the cost is
not. 16 384 particles on the benchmark slope, about 7.5 s simulated at each `dt`, measured on
SwiftShader (the physics does not depend on the device):

| Model | dt | c·dt/dx | centre-of-mass travel at 7 s | mean speed at 7 s |
|---|---|---|---|---|
| Stomakhin (E = 0.14 MPa, c = 18.7 m/s) | 0.01 | 0.015 | 40.19 m | 13.95 m/s |
| | 0.02 | 0.030 | 39.97 m | 13.86 m/s |
| | 0.03 | 0.045 | 39.71 m | 13.81 m/s |
| Cam Clay, Li case V (E = 3 MPa, c = 87 m/s) | 0.005 | 0.035 | 40.86 m | 15.89 m/s |
| | 0.01 | 0.069 | 40.89 m | 15.90 m/s |
| | 0.02 | 0.139 | 40.81 m | 15.89 m/s |

Tripling `dt` for Stomakhin changes the travel by 1.2 %, so the graph preset uses dt = 0.02 s.
The stiff presets (Li 2021, E = 3 MPa) are held under 0.8 × a conservative CFL bound on the
P-wave speed (0.1 · dx / c, `MpmSolverNode::max_stable_dt()`). Cam Clay at 1.4× that bound
agrees with dt = 0.005 to 0.1 %, so the bound could be relaxed; that needs a check of the
deposition phase on real terrain first, which this slope does not reach (§9).

### 3.4 In the app

The app was measured with the kernels before the sorted P2G (Breite Ries, Medium, 2026-09-28):
13–14 substeps per frame at 59 fps in the web build (Balanced pacing, 14 ms GPU per frame,
1.0 ms GPU per substep), 3–6 at 60 fps natively (Smooth, 6 ms). A substep costs about 2× the
benchmark's: the flow on real terrain spreads over more tiles than the slab on the analytic
slope, and the terrain is rendered at the same time. The sorted P2G halves the benchmark's
GPU time per substep at this configuration.

### 3.5 The raw grid view

Handing out the grid velocity to readers ([16-raw-grid-view.md](16-raw-grid-view.md)) needed two
small changes in the solver's grid update, both always on: the node mass is written into
`grid_velocity.w`, and tiles the flow has left are processed once more so their nodes are zeroed.
Neither shows in the total: three A/B runs against the previous kernels at 131 072 particles gave
0.96–1.05× on the GPU time per run, with an identical centre-of-mass trace; the grid update stage
itself is 0.51–0.64 ms against 0.54–0.56 ms. The benchmark's flow moves only 34 m, so few tiles
are left behind; a long run-out leaves more, which has not been measured.

## 4. GPU memory and device limits

| Buffer | Size | Medium | High |
|---|---|---|---|
| particles, and the previous state for interpolation | 2 × 128 B × N | 16.0 MiB | 32.0 MiB |
| grid accumulators | 20 B × R² × L | 23.4 MiB | 31.3 MiB |
| grid velocity | 16 B × R² × L | 18.8 MiB | 25.0 MiB |
| tile flags and particle sort | 4 B × (tiles + 2 × sort tiles + N) | 0.3 MiB | 0.6 MiB |
| column floor, column statistics | 4 B × R² + 24 B × R² | 2.7 MiB | 2.7 MiB |
| density raster and its RGBA8 texture | 8 B × S² | 8.0 MiB | 8.0 MiB |
| **total** | | **69 MiB** | **100 MiB** |

(N particles, R grid columns per side, L layers, S raster edge, 1024 here.) The panel shows the
live figure (`perf_stats().gpu_bytes`).

**Capacity is not the limit** on any device that runs weBIGeo: 4 and 8 GB discrete GPUs and
8 GB unified-memory Macs alike. What differs between devices is memory bandwidth and atomic
throughput, which every substep is bound by:

| Class | Memory bandwidth (approx.) | Expected vs M5 |
|---|---|---|
| Apple M5 (reference) | ~150 GB/s | 1× |
| Apple M1 / M2 base | ~70 / 100 GB/s | 1.5–2× slower |
| NVIDIA laptop, 4–8 GB (RTX 3050 / 4050 / 4060) | 190–260 GB/s | similar or faster, but shares the frame with the terrain |
| Intel Iris Xe / UHD (shared DDR) | 50–70 GB/s | 2–4× slower, weaker atomics |

The panel's sliders reach far beyond the presets (2²¹ particles, 512² × 64 grid). The node
clamps particles and grid to the device's `maxStorageBufferBindingSize` and logs what it did,
because an oversized binding is a validation error that loses the device on the web.

On Windows, a 4 GB card whose memory the tile caches, the browser and other apps fill is paged
to system memory without an error, and everything slows down gradually. This is outside the
solver; Task Manager → GPU → "Dedicated GPU memory" shows it.

## 5. Playback and the GPU queue

The `SimulationDriver` ticks the solver once per frame
([13-simulation-driver.md](13-simulation-driver.md)). On the map it plays in real time (0.1–4×,
positions interpolated between the last two simulated states); in the sandbox and the batch runs it runs as fast as the
pacing budget allows. Pacing sets the GPU time per frame the solver may use: Smooth 6 ms,
Balanced 14 ms, Fast 30 ms. Substeps per frame are the budget divided by the measured GPU time
per substep, which comes from timestamp queries on the solver's own compute pass.

The CPU never waits for the solver: encoding a chunk takes microseconds, and the only readbacks
are asynchronous maps of the 84-byte diagnostics and the timestamps. The contention is on the
GPU's single queue, which WebGPU exposes as one queue per device: a frame's render commands
execute after the solver chunks submitted before them. The pacing bounds that to one chunk per
frame.

Running the solver "in another thread" does not help on the CPU side, since encoding is not the
cost. A second WebGPU device on a worker would let the driver overlap solver and frame, but
buffers cannot be shared between devices: the particles for the 3D renderer (8 MiB at Medium)
would cross the CPU every frame, and on the web it needs Qt-for-WASM threads with
SharedArrayBuffer and COOP/COEP headers. WebGPU has no multi-queue or async compute yet.

## 6. Resource lifetime

The solver holds its GPU resources for as long as the node lives and reallocates only when a
size setting changes:

- The diagnostics are copied into a persistent MapRead buffer inside the last chunk's command
  buffer and mapped from there; no staging buffer or extra submit per run.
- The bind group is cached and rebuilt only when the solver's buffers, the input textures or
  the graph run change.
- One pipeline factory per registry rebuilds the pipelines of the solver instances alive at
  that moment (shader reload, F5); instances deregister in their destructor.
- Asynchronous callbacks carry a liveness token that the destructor clears, so a callback that
  lands after the graph was replaced does nothing. Readbacks are tagged with a reset counter,
  so one that lands after a reset is dropped.
- The energy-line history is capped at 2048 samples (halved when full; the fit is unaffected).

**Soak test, native, Apple M5 (2026-09-29, before the sorted P2G).** 15 minutes of continuous
play from autostart (Breite Ries, Medium, Stomakhin, Smooth pacing), sampled every 10 s:

| | |
|---|---|
| frame rate | 59–60 fps throughout |
| solver GPU memory | **58.6 MiB, constant** (before the interpolation buffer) |
| process memory | 600 MiB at start (tile loading), then 290 → 253 MiB; a sawtooth with **no net growth** |
| GPU per substep | 0.70–0.98 ms, following the flow's tile footprint, no slope |
| errors | none |

The first and second halves differ by 4 % in time per run, from one episode at 400–600 s and
the changing footprint, not from a trend. Not covered: a weak GPU, a long web session, or
repeated resets.

**Outside the solver.** weBIGeo's base code has the same lifetime issues for its other nodes
(`[this]` pipeline factories the registry keeps forever; raw-`this` async callbacks; a command
buffer and queue reference leaked in `RawBuffer::copy_to_buffer(WGPUDevice, …)`). Graph reloads
followed by F5, or a graph replaced mid-run, can crash in those nodes. A fix for the base code
(a registration handle, a liveness token in `Node`, read-backs that do not point back at their
source) is on the branch `fix/node-lifetime-registrations`, not in `main`. `Node::run` and
`complete_run` also log two `qDebug` lines per run; on the web these become `console.log`
calls that DevTools retains.

## 7. Benchmark method

The kernel benchmark in [bench/](bench/README.md) is a standalone WebGPU page that loads the
real WGSL files, mirrors the node's bindings, uniform packing and dispatch sequence, and runs
them on an analytic terrain (a 35° slope bending into a flat runout, with a gully). Every run is
identical work, with no network, tiles or UI. It reports the GPU time per stage from timestamp
queries, the solver memory, the wall time and a centre-of-mass trace.

```sh
npx http-server -c-1 .   → http://localhost:8080/mpm-mls-doc/bench/            # in the browser
node mpm-mls-doc/bench/run_headless.mjs --chrome --baseline <rev> --out r.json  # A/B, headless
```

`--chrome` uses the installed Chrome, which has the real GPU; Playwright's own headless shell
falls back to SwiftShader on macOS. Because P2G accumulates in fixed point, identical kernels
reproduce the trace bit for bit, so a kernel change that alters the physics shows up as a trace
difference next to its speed-up.

To show an improvement: the same machine on mains power, nothing else on the GPU; 3 warm-up runs
and 20–40 measured; the median GPU ms per run, the per-stage split and the trace difference
against the previous commit, with the JSON (it records adapter, user agent, commit and whether
the shaders were modified). Bandwidth- and ALU-bound changes rank differently on different
GPUs, so a slow device (integrated GPU, base M1) is worth a second run.

For memory in the app: play for 10 minutes and sample every 30 s. On the web, use Chrome's Task
Manager (GPU and JS memory of the tab) with DevTools closed; natively, the process memory and,
on macOS, `MTL_HUD_ENABLED=1`. A leak is a slope; thermal throttling on a fanless laptop is a
step in ms per run with flat memory.

## 8. Context: what makes real time possible

- **Resolution.** Li et al. 2021, the closest prior work (same method family, Cohesive Cam
  Clay, real 3D terrain), use 0.5 m cells and 8 particles per cell on CPUs, far from real time.
  This solver uses 5–12.5 m cells (up to 25³ ≈ 15 000× fewer cells per volume) and a larger
  time step. That trade, more than the GPU, is what makes interactive rates possible, and it is
  why the released slab is thinner than a cell ([05-tuning.md](05-tuning.md)).
- **MLS-MPM** (Hu et al. 2018) folds the stress and APIC terms into one affine matrix, so P2G
  needs no weight gradients.
- **GPU MPM practice** (Gao et al. 2018; Wang et al. 2020; Hu's `taichi_mpm`): particles binned
  by cell, P2G reduced before the global atomic, a sparse grid allocated only where particles
  are, and G2P fused with the next P2G. The active tiles (§2.1) are the WebGPU-sized form of the
  sparse grid, and the sorted P2G (§2.2) is the binning and the reduction. The fusion is open
  (§9).
- **CK-MPM** (Liu et al. 2025) would cut the node accesses per particle below the 27 of the
  quadratic B-spline; it changes only the transfer kernels.

## 9. Further improvements

| # | Idea | Expected effect | Effort | Notes |
|---|---|---|---|---|
| 1 | **Fuse G2P and the next P2G** (G2P2G, Wang et al. 2020) | one particle read and write per substep instead of two; G2P is now the largest stage | M | Needs a double-buffered grid (read step n's velocities while accumulating step n+1) |
| 2 | **dt from a relaxed CFL bound** (0.1 → ~0.2 · dx / c, capped at ~0.03 s) | ~2× simulated time per substep for the stiff Li / Cam Clay presets | S | §3.3; check the deposition on real terrain first |
| 3 | **Leaner particle**: f16 for C and τ, a flag bit instead of `mass` | particle bandwidth −30–40 % | M | Watch the APIC precision; F must stay f32 |
| 4 | **Subgroup reductions** in P2G | fewer workgroup-memory atomics | S–M | Behind the WGSL `subgroups` feature, with a fallback |
| 5 | **Indirect dispatch over a compacted tile list** | no idle grid-update workgroups | S–M | Only worth it for domains ≫ 320² |
| 6 | **CK-MPM kernel** | fewer node accesses per particle, less diffusion | L | The proposal's stretch goal |
| 7 | Second device on a worker (§5) | solver and frame overlap | L | After 1–3 |

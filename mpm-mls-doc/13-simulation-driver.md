# Simulation driver: real time and accuracy playback

How a time-stepped simulation is played back frame by frame (2026-10-24). Before this, the
avalanche panel stepped the MPM solver from `AvalanchePanel::draw()`: UI code, which stopped
with the GUI hidden, and one pacing that ran the simulation as fast as the GPU budget allowed.
That suits the sandbox (accuracy, fastest result) but not a "cinema" view on the map, where the
avalanche should move at a steady, chosen speed.

## Decisions

| Topic | Decision |
|---|---|
| Architecture | An interface in `webgpu_compute` that time-stepped nodes implement; an app-level driver ticks every implementing node each frame; panels only control. |
| Falling behind | Fixed-step loop. `dt` never scales with the frame time (explicit solver, CFL bound). When the GPU cannot keep up, simulated time falls behind and the panel says so. |
| Modes | Map: real time. Sandbox and batch: accuracy (the old pacing). A switch in the panel changes either. |
| Speed | 0.1x to 4x, default 1x. |
| Interpolation | Real time only, switchable ("Interpolate", on by default). Positions are blended between the last two simulated states, as game engines do; accuracy shows the latest state. |
| Other simulations | Interface generic, MPM the only adopter. The trajectory model is one-shot and stays a plain node. |
| Frame cap | Pacing (Smooth 6 ms / Balanced 14 ms / Fast 30 ms GPU per frame) caps both modes. |

## Pieces

```
App::render() ── every frame, GUI shown or not, before the frame is painted
   └─ ImGuiManager::tick_simulations() → SimulationDriver::tick(graph)    wall time since the last frame
        └─ for each node that is a TimeSteppedSimulation:
             clock().tick(simulation, wall)
                ├─ accuracy:  advance(steps, chunks)                       chained by the simulation
                └─ real time: advance_frame(steps, end_run), set_interpolation(alpha)
   └─ playing → repaint this frame
   └─ paint: AvalancheRenderer draws mix(previous, current, alpha)

AvalanchePanel::draw() ── when the GUI is shown
   └─ sync_clock(solver): mode, speed, interpolation, budget; play / pause / step
```

| File | What |
|---|---|
| `webgpu/compute/TimeSteppedSimulation.h` | The interface: `can_advance`, `is_advancing`, `advance(steps, chunks)`, `advance_frame(steps, end_run)`, `set_interpolation`, `time_step`, `simulated_time`, `gpu_ms_per_step`, `at_rest`, `reset`, and `clock()`. The clock lives *in* the simulation, so it goes with the node when a graph is replaced and a new node starts paused. |
| `webgpu/compute/SimulationClock.h/.cpp` | Per-simulation playback: play/pause/step, mode, speed, interpolation, budget, the presentation time, the speed measurement. Plain C++, no GPU or Qt. |
| `webgpu/compute/SimulationDriver.h/.cpp` | Finds the implementing nodes (`dynamic_cast`) and ticks their clocks with the measured frame time; reports whether one is playing. |
| `unittests/webgpu_compute/test_SimulationClock.cpp` | The clock against a fake simulation (`unittests_webgpu_compute`). |
| `MpmSolverNode` | Implements the interface (see below). |
| `AvalancheRenderer` + shaders | Draw the positions blended between the previous and the current particles. |
| `App`, `ImGuiManager` | Tick before the paint; repaint every frame while a simulation plays. |
| `AvalanchePanel` | Playback controls and `sync_clock()`; the sandbox logic (at-rest stop, export, batch) on top of the clock. |

## The two modes

**Accuracy** — unchanged from the old `apply_pacing()`. Chunk = budget / GPU ms per step
(1–64, with 1/8 hysteresis); run = max(2 chunks, 24 steps); a new run whenever the previous one
is done. `advance()`: the solver submits two chunks at once and each further chunk from the
previous one's work-done callback. Simulated time runs at whatever rate the GPU manages (2–3x
real time on the M5 map scenario, far more in the sandbox). No interpolation.

**Real time** — a fixed-step loop with interpolation (Fiedler's "Fix your timestep!"), one
chunk per frame:

```
T     += wall_dt × speed                          presentation time (wall_dt clamped to 0.25 s)
steps  = ceil((T − S) / dt),  at most the budget  S: the simulation's time
advance_frame(steps, end_run = every 8th chunk)   S moves to S + steps × dt
T      = min(T, S + dt)                           what the budget could not simulate is dropped
alpha  = (T − S_before) / (S_after − S_before)    of the last chunk; 1 without interpolation
```

- **Always bracketed.** S is at or past T after every frame, and the last chunk started before
  T, so the frame can show exactly time T: positions blended between the particles before and
  after the last chunk. With steps longer than a frame (0.1x at `dt` = 5 ms, a step every three
  frames) the simulation stands still in most frames, the picture moves in every one.
- **Runs.** The chunks still form runs, closed every 8 chunks (`end_run`), so the overlay, the
  diagnostics readback and the downstream nodes run ~7.5 times per second. A run counts as
  done as soon as its last chunk is *submitted*: the readback is mapped right away
  (`mapAsync` itself waits for the GPU), so the next frame opens the next run without a gap.
  A pause or a switch to accuracy closes an open run with an empty chunk, so Step, Reset and
  graph runs never wait behind it.
- **Budget.** `budget_steps = budget_ms / best GPU ms per step since the reset`, fractional and
  carried between frames (a credit of at most budget + 1 steps), so 3.5 steps per frame is not
  rounded down to 3. Why the *best*, not the average: see the measurements. Real overload shows
  as backpressure: `advance_frame()` refuses while three chunks are in flight.
- **Behind.** Measured speed over 1 s windows; "behind" below 90 % of the target, cleared above
  95 % (it flickered without the hysteresis).
- **Resync.** When something other than the clock's own chunks moved the simulation (the first
  frame, a Step, a reset run), T starts again from the simulation's time.

## MPM solver

`advance()` overrides `substeps_per_run` / `substeps_per_submit` for one run without writing
them, so the settings stay what a plain `rerun()` (the node editor's Step) and Manual pacing
use. Before, `apply_pacing()` wrote them every frame. `submit_chained_chunk()` takes its
share of a chained run's substeps (spread evenly); `advance_frame()` opens a run (through
`rerun()`) or submits the next chunk of the open one; `close_frame_run()` ends it.
`on_chunk_done(slot, substeps, run_serial)` only continues the current chained run.

**Previous particles.** Before every real-time chunk, `submit_chunk()` copies the particle
buffer into `m_previous_particles` (one `copyBufferToBuffer`, 8 MB at Medium, 16 MB at High;
the buffer doubles the particle memory). The particle view exposes it with the interpolation
(`previous_particles`, `interpolation`). Not on the reseeding chunk (the copy would hold the old
flow; the seed runs inside the compute pass) and not in accuracy runs: then it is null and the
renderer draws the current state. An empty closing chunk leaves it as it is.

## Rendering

`render_avalanche.wgsl`, `render_avalanche_powder.wgsl` and `avalanche_surface_splat.wgsl` read
the position as `mix(previous[i].xyz, particles[i].xyz, interpolation)`; velocity, strain rate
and mass come from the current state. Without previous particles the current buffer is bound
in their place and the interpolation is 1. So the spheres, the powder and the flow surface (and
its track) all move with the presentation time; the 2D density overlay stays per run.

**Ordering.** The driver ticks at the start of `App::render()`, before the paint: the chunk is
submitted, then the renderer writes its uniforms (with this frame's interpolation), then the
frame's command buffer reads both. Before, it ticked in the GUI pass after the paint, so a frame
would have drawn the new chunk with the previous frame's interpolation.

**Repaint.** The map only repaints on request. `ParticleRenderNode` requested it when the solver
completed a run, so the 3D view updated once per run — every 8 frames in real time before this
change, every 2–3 frames in accuracy. Now `tick_simulations()` reports a playing simulation and
the app repaints every frame.

## Panel

"Playback" (Real time / Accuracy), "Speed" and "Interpolate" (real time only) and "Pacing",
under the transport. Mode and pacing are remembered per context (`playback/map_mode`,
`playback/map_pacing`, `playback/sandbox_mode`, `playback/sandbox_pacing`), speed and
interpolation once (`playback/speed`, `playback/interpolate`). Defaults: map real time +
Smooth, sandbox accuracy + Balanced, interpolation on; the old single `playback/pacing` carries
over to the sandbox. Batch mode always plays at accuracy, Fast. Readouts: "Speed: 1.00x real
time, budget 8.7 substeps/frame", or in orange "… – behind 4.00x" with the reason in the
tooltip, or in accuracy "Speed: 2.4x real time, 16 substeps/frame".

## Measurements (M5, native release, map scenario Hintertux, Medium, `dt` 5.04 ms)

55 s per run, logged once per second by a temporary harness (reverted). "Frozen" = frames in
which the simulated time did not change; "still" = frames in which the *drawn* time did not
change; "drawn per frame" = the drawn time's advance per frame, mean ± standard deviation;
"repaints" = frames the map was actually painted.

**Final version** (interpolation, tick before paint, repaint while playing):

| Setting | Speed | fps / repaints | Frozen | Still | Drawn per frame |
|---|---|---|---|---|---|
| 1x Smooth (GUI hidden 30–40 s) | 1.00x throughout | 60 / 60 | 0 | 0 | 16.67 ± 0.3 ms |
| 0.1x Smooth | 0.10x | 60 / 60 | 41 per second | **0** | 1.667 ± 0.01 ms |
| 0.1x Smooth, interpolation off | 0.10x | 60 / 60 | 41 | **41** | 0 or 5 ms |
| 4x Smooth | 2.67x steady, "behind" | 60 / 60 | 0 | 0 | 44.5 ± 1.8 ms |
| accuracy, Balanced | 2.5–3.5x | 45 / 45 | 30 | 30 | — |

**On the way** (simulated time only; the map then repainted once per run, which these numbers
did not show):

| Version | Setting | Speed | fps | Frozen frames/s |
|---|---|---|---|---|
| 2-chunk chained runs | 1x Smooth | 0.91x, then 1.00x with fractional budget; 0.6x late in the flow | 60 | **30** (both chunks in one frame, none in the next) |
| 8-frame runs, average cost | 1x Smooth | 0.5x | 54–56 | 1–3 |
| 8-frame runs, best cost relaxing (10 s) | 1x Smooth | 1.0x → 0.58x over 50 s | 55–60 | 0 |
| 8-frame runs, best cost since reset | 1x / 4x Smooth | 1.00x / 2.64x | 56–60 | 0 |

- **GUI hidden:** the simulation keeps advancing at 1.0x.
- **GPU cost per step depends on the load.** The same flow measures 0.8 ms/step in accuracy
  chunks of ~16 steps and 1.5–3 ms/step in per-frame chunks of 2–4. At 1x and 2x (Balanced)
  the measured GPU time per frame was the same ~9.5 ms although the steps differed 2x — the
  GPU appears to clock down when it has little to do (not proven; per-pass overhead or overlap
  with rendering would look similar). Budgeting with the average cost then lowers the load,
  which raises the measured cost: a feedback loop down to 0.5x. With the best cost since the
  reset, 1x holds. With per-frame repaints the GPU is busier and measures 1.0–1.2 ms/step.
- **Accuracy unchanged** (batch, avaKot, Stomakhin Coulomb, as in phase 6 of
  [11-avaframe-comparison.md](11-avaframe-comparison.md)): at rest after 80.25 s (phase 6:
  79.60 s), footprint pft > 0.1 m 12.51 ha (12.51 ha), max pfv 54.01 m/s (54.01), wall 3.37 s
  (3.32 s). The stop time moves with the run boundaries, since the at-rest check runs once
  per run.
- Unit tests: 23 sections (real time against the wall clock at 20–144 fps and 0.1–4x; drawn
  time exactly one frame further every frame, also at 0.1x; interpolation off; falling behind
  within the budget with even motion; fractional budget; best cost; hysteresis; stalls; a GPU
  that refuses chunks; reset; pause and mode switch close the open run; step; accuracy run
  sizes; chunk hysteresis). Forcing the interpolation to 1 fails the two drawn-time sections;
  removing the run-size cap failed the earlier version.
- Web build: tested in the browser by Manuel (25 Oct): real-time playback and interpolation work.

## Not done / open

- The diagnostics, the energy line and the 2D overlay update once per run: ~7.5 Hz in real time.
- In real time a heavier flow than the best cost suggests is only caught by the backpressure,
  so the frame rate can dip slightly before the simulation falls behind.
- Accuracy playback is not interpolated (its runs are large and their timing irregular).
- A particle that leaves the domain disappears at once (mass 0 in the current state), not
  interpolated out.

## Adding another simulation

A step-by-step guide that is not MPM specific (the contract per method, a sketch of a node,
wiring, interpolation, pitfalls, testing):
[docs/time_stepped_simulations.md](../docs/time_stepped_simulations.md). In short:

1. Derive the node from `Node` and `TimeSteppedSimulation`; implement the methods.
   `advance()` usually stores the request and calls `rerun()`; `advance_frame()` opens a run
   the same way or submits the next chunk of the open one; `set_interpolation()` is optional
   (default: ignored), for a renderer that can blend.
2. Nothing to register: `SimulationDriver` finds it in the active graph by `dynamic_cast`.
3. A panel controls it through `clock()`: `set_settings()` (mode, speed, interpolation,
   budget), `play()`, `pause()`, `step()`, and the readouts `measured_speed()`, `behind()`,
   `steps_per_frame_budget()`.

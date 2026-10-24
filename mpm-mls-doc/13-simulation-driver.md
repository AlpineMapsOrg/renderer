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
| Falling behind | Fixed-step accumulator. `dt` never scales with the frame time (explicit solver, CFL bound). When the GPU cannot keep up, simulated time falls behind and the panel says so. |
| Modes | Map: real time. Sandbox and batch: accuracy (the old pacing). A switch in the panel changes either. |
| Speed | 0.1x to 4x, default 1x. |
| Interpolation | None (yet). A frame shows the state after its chunk. |
| Other simulations | Interface generic, MPM the only adopter. The trajectory model is one-shot and stays a plain node. |
| Frame cap | Pacing (Smooth 6 ms / Balanced 14 ms / Fast 30 ms GPU per frame) caps both modes. |

## Pieces

```
ImGuiManager::render() ── every frame, GUI shown or not
   └─ SimulationDriver::tick(graph)            wall time since the last frame
        └─ for each node that is a TimeSteppedSimulation:
             clock().tick(simulation, wall)     decides how many steps, starts runs
                └─ simulation.advance(steps, chunks, submission) / submit_frame()

AvalanchePanel::draw() ── when the GUI is shown
   └─ sync_clock(solver): mode, speed, budget from the panel; play / pause / step
```

| File | What |
|---|---|
| `webgpu/compute/TimeSteppedSimulation.h` | The interface: `can_advance`, `is_advancing`, `advance(steps, chunks, submission)`, `submit_frame`, `time_step`, `simulated_time`, `gpu_ms_per_step`, `at_rest`, `reset`, and `clock()`. The clock lives *in* the simulation, so it goes with the node when a graph is replaced and a new node starts paused. |
| `webgpu/compute/SimulationClock.h/.cpp` | Per-simulation playback: play/pause/step, mode, speed, budget, the accumulator, the speed measurement. Plain C++, no GPU or Qt. |
| `webgpu/compute/SimulationDriver.h/.cpp` | Finds the implementing nodes (`dynamic_cast`) and ticks their clocks with the measured frame time. |
| `unittests/webgpu_compute/test_SimulationClock.cpp` | The clock against a fake simulation (`unittests_webgpu_compute`). |
| `MpmSolverNode` | Implements the interface (see below). |
| `AvalanchePanel` | Playback controls and `sync_clock()`; the sandbox logic (at-rest stop, export, batch) on top of the clock. |

## The two modes

**Accuracy** — unchanged from the old `apply_pacing()`. Chunk = budget / GPU ms per step
(1–64, with 1/8 hysteresis); run = max(2 chunks, 24 steps); a new run whenever the previous one
is done. `Submission::Chained`: the solver submits two chunks at once and each further chunk
from the previous one's work-done callback. Simulated time runs at whatever rate the GPU
manages (2–3x real time on the M5 map scenario, far more in the sandbox).

**Real time** — a fixed-step loop:

```
debt += wall_dt × speed                         (wall_dt clamped to 0.25 s)
when the previous run is done:
    steps  = floor((debt + 7 frames × speed × frame_time) / dt),  at most 8 × budget_steps
    advance(steps, min(steps, 8), PerFrame)     one chunk per frame, steps spread evenly
    debt  -= steps × dt,  at most one run's worth kept (no spiral of death)
while it runs:  submit_frame() each frame
```

- **One chunk per frame.** A run spans 8 frames; its first chunk goes out in `advance()`, the
  others from `submit_frame()`, one per frame. The steps are split Bresenham-style (chunk *i*
  of *n* gets ⌊(i+1)S/n⌋ − ⌊iS/n⌋), so neighbouring frames differ by at most one step.
- **No gap between runs.** A PerFrame run counts as done as soon as its last chunk is
  *submitted*: the state readback is mapped right away (`mapAsync` itself waits for the GPU),
  so the next run starts in the next frame. Late chunk callbacks carry a run serial and never
  drive a later run.
- **Budget.** `budget_steps = budget_ms / best GPU ms per step since the reset` (fractional,
  so 3.5 steps per frame is not rounded down to 3). Why the *best*, not the average: see the
  measurements. Real overload shows as backpressure instead: `submit_frame()` skips the frame's
  chunk while three chunks are in flight.
- **Behind.** Measured speed over 1 s windows; "behind" below 90 % of the target, cleared above
  95 % (it flickered without the hysteresis).
- **Pause** finishes a run that is under way (at most 7 more chunks), so Step, Reset and graph
  runs are never stuck behind it.
- **Reset** (simulated time going back) drops the debt and the best cost.

Without interpolation, speeds where one step is longer than a frame (0.1x at `dt` = 5 ms) show
some unchanged frames; that is inherent until interpolation exists.

## MPM solver

`advance()` overrides `substeps_per_run` / `substeps_per_submit` for one run without writing
them, so the settings stay what a plain `rerun()` (the node editor's Step) and Manual pacing
use. Before, `apply_pacing()` wrote them every frame. `submit_chunk()` takes its share of the
run's substeps; `run_impl()` submits one chunk (PerFrame) or two (Chained);
`on_chunk_done(slot, substeps, run_serial)` only continues the current chained run.

## Panel

"Playback" (Real time / Accuracy), "Speed" (real time only) and "Pacing", under the transport.
Mode and pacing are remembered per context (`playback/map_mode`, `playback/map_pacing`,
`playback/sandbox_mode`, `playback/sandbox_pacing`), the speed once (`playback/speed`).
Defaults: map real time + Smooth, sandbox accuracy + Balanced; the old single
`playback/pacing` carries over to the sandbox. Batch mode always plays at accuracy, Fast.
Readouts: "Speed: 1.00x real time, budget 8.7 substeps/frame", or in orange "… – behind 4.00x"
with the reason in the tooltip, or in accuracy "Speed: 2.4x real time, 16 substeps/frame".

## Measurements (M5, native release, map scenario Hintertux, Medium, `dt` 5.04 ms)

55 s per run, logged once per second by a temporary harness in `ImGuiManager::render()`
(reverted). "Frozen" = frames in which the simulated time did not change.

| Version | Setting | Speed | fps | Frozen frames/s |
|---|---|---|---|---|
| 2-chunk chained runs | 1x Smooth | 0.91x, then 1.00x with fractional budget; 0.6x late in the flow | 60 | **30** (both chunks in one frame, none in the next) |
| per frame, average cost | 1x Smooth | 0.5x | 54–56 | 1–3 |
| per frame, best cost relaxing (10 s) | 1x Smooth | 1.0x → 0.58x over 50 s | 55–60 | 0 |
| **final** | 1x Smooth | **0.98–1.01x** throughout | 56–58 (60 with the GUI hidden) | **0** |
| **final** | 4x Smooth | 2.64x steady, "behind" | 60 | 0 |
| per frame, throttle at 2 | 2x Balanced | 2.0x | 60 | 4–7 |
| accuracy | Balanced | 2.1–2.7x | 45 | 30 |

- **GUI hidden** (30–40 s of the 1x run): the simulation kept advancing at 1.0x.
- **GPU cost per step depends on the load.** The same flow measures 0.8 ms/step in accuracy
  chunks of ~16 steps and 1.5–3 ms/step in per-frame chunks of 2–4. At 1x and 2x (Balanced)
  the measured GPU time per frame was the same ~9.5 ms although the steps differed 2x — the
  GPU appears to clock down when it has little to do (not proven; per-pass overhead or overlap
  with rendering would look similar). Budgeting with the average cost then lowers the load,
  which raises the measured cost: a feedback loop down to 0.5x. With the best cost since the
  reset, 1x holds.
- **Accuracy unchanged** (batch, avaKot, Stomakhin Coulomb, as in phase 6 of
  [11-avaframe-comparison.md](11-avaframe-comparison.md)): at rest after 80.25 s (phase 6:
  79.60 s), footprint pft > 0.1 m 12.51 ha (12.51 ha), max pfv 54.01 m/s (54.01), wall 3.37 s
  (3.32 s). The stop time moves with the run boundaries, since the at-rest check runs once
  per run.
- Unit tests: 17 sections (real time against the wall clock at 20–144 fps and 0.25–4x, falling
  behind within the budget, fractional budget, best cost, hysteresis, stalls, reset, pause,
  step, accuracy run sizes, chunk hysteresis). Removing the cap on the run size makes them
  fail (checked once, by hand).
- Web build compiles; not run in the browser.

## Not done / open

- **Interpolation** between steps (render-side; needs the previous particle positions).
- The diagnostics, the energy line and the 2D overlay update once per run: ~7.5 Hz in real time.
- In real time a heavier flow than the best cost suggests is only caught by the backpressure,
  so the frame rate can dip slightly before the simulation falls behind.

## Adding another simulation

1. Derive the node from `Node` and `TimeSteppedSimulation`; implement the eight methods.
   `advance()` usually stores the request and calls `rerun()`; `submit_frame()` submits the
   next chunk of a PerFrame run.
2. Nothing to register: `SimulationDriver` finds it in the active graph by `dynamic_cast`.
3. A panel controls it through `clock()`: `set_settings()` (mode, speed, budget), `play()`,
   `pause()`, `step()`, and the readouts `measured_speed()`, `behind()`,
   `steps_per_frame_budget()`.

# Time-stepped simulations

How to add a simulation that runs on the GPU in fixed time steps and plays back frame by frame,
in real time or as fast as possible. The MLS-MPM avalanche solver (`MpmSolverNode`) is the
first user, but nothing in the interface is MPM or avalanche specific. A heat or wave
equation, a cellular automaton, an advected particle cloud or a shallow-water model all fit.

Use it for anything **advanced step by step over time**. A node that computes its result
once per graph run (normals, a trajectory, a texture) stays a plain `Node`.

## Overview

```
App::render()                                   every frame, GUI shown or hidden, before the paint
 └─ ImGuiManager::tick_simulations()
     └─ SimulationDriver::tick(graph)           wall time since the last frame
         └─ for every node in the graph that is a TimeSteppedSimulation:
              simulation.clock().tick(simulation, wall)
                 ├─ Accuracy:  simulation.advance(steps, chunks)
                 └─ RealTime:  simulation.advance_frame(steps, end_run)
                               simulation.set_interpolation(alpha)
 └─ any clock playing or a run under way → the frame is repainted

your panel (optional)                           only controls the clock: play, pause, step, settings
```

| File | Role |
|---|---|
| `webgpu/compute/TimeSteppedSimulation.h` | The interface your node implements. Also holds the node's `SimulationClock`. |
| `webgpu/compute/SimulationClock.h/.cpp` | Decides each frame *how many* steps to run. Plain C++, no GPU, no Qt. |
| `webgpu/compute/SimulationDriver.h/.cpp` | Finds the simulations in the active graph (`dynamic_cast`) and ticks their clocks. |
| `apps/webgpu_app/App.cpp`, `ImGuiManager.cpp` | Call the driver before painting, and repaint while something plays. |
| `unittests/webgpu_compute/test_SimulationClock.cpp` | The clock against a `FakeSimulation`: a good second reference next to the MPM solver. |

**Division of labour:** the clock owns *time* (when, how many steps, at what speed, how far to
interpolate). The simulation owns *work* (submitting GPU passes, chaining chunks, knowing when
the GPU is done). The UI only flips switches on the clock.

## The two playback modes

| | **Accuracy** | **RealTime** |
|---|---|---|
| Goal | The result as fast as possible (batch runs, comparisons). | Simulated time follows the wall clock × `speed` (0.1×–4×), like a game engine's fixed-step loop. |
| What the clock calls | `advance(steps, chunks)`: a whole run, which the simulation chains at its own pace. | `advance_frame(steps, end_run)`: one chunk per frame, submitted now. |
| Step count | Sized from the GPU budget (`budget_ms` ÷ `gpu_ms_per_step()`), 1–64 per chunk; a run is at least 24 steps. | Just enough to reach the presentation time, capped by the budget. Time the budget cannot cover is dropped: the simulation falls *behind* (`behind()`), it never piles up work. |
| Interpolation | None (`set_interpolation(1)`). | `alpha` = where the presentation time lies between the states before and after the last chunk. |

The time step never changes. An explicit solver is bound by its stability limit (CFL), so the
only thing the clock varies is *how many* steps run.

## The interface, method by method

```cpp
class TimeSteppedSimulation {
public:
    virtual bool can_advance() const = 0;
    virtual bool is_advancing() const = 0;
    virtual void advance(uint32_t steps, uint32_t chunks) = 0;
    virtual bool advance_frame(uint32_t steps, bool end_run) = 0;
    virtual void set_interpolation(float) { }
    virtual float time_step() const = 0;
    virtual float simulated_time() const = 0;
    virtual float gpu_ms_per_step() const = 0;
    virtual bool at_rest() const { return false; }
    virtual void reset() = 0;
    SimulationClock& clock();
};
```

| Method | Contract |
|---|---|
| `can_advance()` | The inputs are in place (sockets connected, data present). While false, a playing clock pauses itself. |
| `is_advancing()` | True from `advance()` until that run is done, and while a real-time run is open. The clock never starts a second run while it is true. |
| `advance(steps, chunks)` | Accuracy. Start a run of `steps` steps split into `chunks` GPU submissions (1 ≤ chunks ≤ steps), spread evenly: they differ by at most one step. Return at once; chain the chunks yourself, typically each from the previous one's work-done callback. |
| `advance_frame(steps, end_run)` | RealTime. Submit one chunk of `steps` steps **now**, opening a run if none is open. `end_run` closes the run with this chunk; `steps` may be 0 then (the clock closes a run that way on pause or a mode switch). Return **false and submit nothing** when the GPU is too far behind, or an `advance()` run is under way. The clock retries next frame. |
| `set_interpolation(alpha)` | Optional. What the next frame should show: 0 = the state before the last chunk, 1 = after it. Ignore it if your renderer cannot blend. |
| `time_step()` | The fixed step [s]. |
| `simulated_time()` | Simulated seconds since the last reset. **Advance it when a chunk is submitted**, not when the GPU finishes: the clock reads it right after `advance_frame()`. Back to 0 on a reset. |
| `gpu_ms_per_step()` | Measured GPU time per step (timestamp queries), 0 until measured. While it is 0, the clock uses the manual step counts from its settings. |
| `at_rest()` | Optional: the simulation has settled, by its own criterion. The clock does not use it; panels can (the avalanche sandbox stops on it). |
| `reset()` | Back to the initial state, taking effect with the next run. The clock notices the jump in `simulated_time()` and resyncs. |
| `clock()` | Not virtual. The clock lives inside the simulation, so it is destroyed with the node when the graph is replaced, and a new node starts paused. |

## A run, and why it matters

A **run** is one `Node` run: it starts with `run()` / `rerun()` → `run_impl()` and ends with
`complete_run()`. `NodeGraph` chains its nodes in topological order on `run_completed`, so
**every completed run of your simulation re-runs everything downstream of it** in the graph:
overlays, rasterisation, statistics.

So:
- In Accuracy a run is a whole `advance()`.
- In RealTime a run spans several frames: the clock closes it every 8 chunks
  (`SimulationClock::REAL_TIME_RUN_CHUNKS`). Downstream work then happens about 7.5 times a
  second at 60 fps, not every frame.

A renderer that should update **every frame** must not depend on the graph re-running. Give it
direct access to your GPU buffers instead (a callback, or an output socket that it reads each
frame), as `ParticleRenderNode` does for the avalanche.

## Implementing one: a sketch

A 2D heat equation on a grid as the example: the explicit scheme is stable for
`dt ≤ dx² / (4α)`, so it has the same "fixed step, vary the count" character as the MPM solver.
The sketch leaves out allocation and the shader. The structure is the point.

```cpp
// webgpu/compute/nodes/HeatDiffusionNode.h
class HeatDiffusionNode : public Node, public TimeSteppedSimulation {
    Q_OBJECT
public:
    NODE_TYPE_NAME(HeatDiffusionNode)
    explicit HeatDiffusionNode(webgpu::Context& ctx);

    // TimeSteppedSimulation
    bool can_advance() const override;
    bool is_advancing() const override { return is_running(); }
    void advance(uint32_t steps, uint32_t chunks) override;
    bool advance_frame(uint32_t steps, bool end_run) override;
    void set_interpolation(float alpha) override { m_interpolation = alpha; }
    float time_step() const override { return m_dt; }
    float simulated_time() const override { return m_time; }
    float gpu_ms_per_step() const override { return m_gpu_ms_per_step; }
    void reset() override { m_reset_pending = true; }

public slots:
    void run_impl() override;

private:
    void submit_chunk(uint32_t steps, bool last_chunk);
    void submit_next_chained_chunk();
    void on_chunk_done(uint64_t run_serial);

    // what the clock asked for, consumed by the next run_impl()
    struct Request { uint32_t steps = 0, chunks = 0; bool per_frame = false, end_run = false; } m_request;

    float m_dt = 0.01f, m_time = 0.0f, m_gpu_ms_per_step = 0.0f, m_interpolation = 1.0f;
    bool m_reset_pending = true;
    bool m_frame_run_open = false;
    bool m_run_per_frame = false;        // the current run is a real-time one
    uint32_t m_steps_left = 0, m_chunks_left = 0;
    uint64_t m_run_serial = 0;           // tells a late callback whether its run is still current
    uint32_t m_in_flight = 0;            // submitted chunks the GPU has not finished
    static constexpr uint32_t MAX_IN_FLIGHT = 3;
    std::shared_ptr<HeatDiffusionNode*> m_alive = std::make_shared<HeatDiffusionNode*>(this); // nulled in the destructor
};
```

```cpp
// webgpu/compute/nodes/HeatDiffusionNode.cpp
void HeatDiffusionNode::advance(uint32_t steps, uint32_t chunks)
{
    m_request = { steps, chunks, false, false };
    rerun(); // -> run_impl(), with the graph's last context
}

bool HeatDiffusionNode::advance_frame(uint32_t steps, bool end_run)
{
    if (m_in_flight >= MAX_IN_FLIGHT)
        return false; // the GPU is behind: drop this frame's chunk, don't queue it
    if (!m_frame_run_open) {
        if (is_running() || steps == 0)
            return false; // an advance() run is under way, or nothing to open a run with
        m_request = { steps, 1, true, end_run };
        const uint64_t serial = m_run_serial;
        rerun();
        return m_run_serial != serial; // false if the run did not start (inputs missing, node disabled)
    }
    submit_chunk(steps, end_run); // the next chunk of the open run
    if (end_run) {
        m_frame_run_open = false;
        complete_run(); // downstream nodes run once per real-time run
    }
    return true;
}

void HeatDiffusionNode::run_impl()
{
    const Request request = std::exchange(m_request, {});
    if (!can_advance()) {
        fail_run("HeatDiffusionNode: inputs not ready");
        return;
    }
    if (m_reset_pending) {
        write_initial_state(); // upload the initial field
        m_time = 0.0f;
        m_reset_pending = false;
    }
    m_run_serial++;
    m_run_per_frame = request.per_frame;
    if (request.per_frame) {
        m_frame_run_open = !request.end_run;
        submit_chunk(request.steps, request.end_run);
        if (request.end_run)
            complete_run();
        return; // later chunks arrive through advance_frame()
    }
    m_steps_left = std::max(request.steps, 1u);
    m_chunks_left = std::clamp(request.chunks, 1u, m_steps_left);
    submit_next_chained_chunk();
    if (m_steps_left > 0)
        submit_next_chained_chunk(); // two in flight, so the GPU never idles between chunks
}

void HeatDiffusionNode::submit_next_chained_chunk()
{
    const uint32_t steps = m_steps_left / m_chunks_left; // the remaining steps spread evenly
    m_steps_left -= steps;
    m_chunks_left--;
    submit_chunk(steps, m_steps_left == 0);
}

void HeatDiffusionNode::submit_chunk(uint32_t steps, bool last_chunk)
{
    // Real time: copy the current field to m_previous first, if the renderer blends.
    // Encode `steps` dispatches (ping-pong between two field buffers), with timestamp queries
    // around them, and submit one command buffer.
    // ...
    m_time += float(steps) * m_dt; // at submission: the clock reads it right away
    m_in_flight++;

    struct Done { std::shared_ptr<HeatDiffusionNode*> alive; uint64_t serial; };
    WGPUQueueWorkDoneCallbackInfo info {};
    info.mode = WGPUCallbackMode_AllowProcessEvents; // fires from the app's event pump
    info.callback = [](WGPUQueueWorkDoneStatus, WGPUStringView, void* userdata, void*) {
        std::unique_ptr<Done> done(static_cast<Done*>(userdata));
        if (HeatDiffusionNode* self = *done->alive) // the node may be gone by now
            self->on_chunk_done(done->serial);
    };
    info.userdata1 = new Done { m_alive, m_run_serial };
    wgpuQueueOnSubmittedWorkDone(m_ctx->queue(), info);
}

void HeatDiffusionNode::on_chunk_done(uint64_t run_serial)
{
    m_in_flight--;
    // read back the timestamps -> m_gpu_ms_per_step
    if (run_serial != m_run_serial || m_run_per_frame)
        return; // a late callback of an earlier run, or a real-time run (advance_frame drives it)
    if (m_steps_left > 0) {
        submit_next_chained_chunk();
        return;
    }
    if (m_in_flight == 0)
        complete_run(); // the accuracy run is done
}
```

The MPM solver does the same with more on top: a state readback before `complete_run()`, a
reseed on reset, and keeping the previous particles for interpolation. See
`MpmSolverNode::advance`, `advance_frame`, `run_impl`, `submit_chained_chunk`, `submit_chunk`
and `on_chunk_done`.

### Wiring it up

1. **The node:** derive from `Node` and `TimeSteppedSimulation` in `webgpu/compute/nodes/`.
2. **Register it** in `NodeRegistry::NodeRegistry()` (`webgpu/compute/NodeRegistry.cpp`) and
   add it to a graph preset (`apps/webgpu_app/resources/graphs/*.json`). That is all for
   playback: **there is no driver registration**. `SimulationDriver` finds every implementing
   node in the active graph each frame.
3. **Show it:** connect an output socket to `OverlayRenderNode` for a 2D texture on the map
   (updates once per run). For every-frame updates, write an app-side render node that hands
   your buffers to an engine renderer, like `ParticleRenderNode` → `AvalancheRenderer`.
4. **Control it** (optional): a panel finds the node and talks to `clock()`:
   - `set_settings({ mode, speed, interpolate, budget_ms, ... })`, `play()`, `pause()`,
     `step()` (one accuracy-sized run, playing or not);
   - readouts: `playing()`, `measured_speed()`, `behind()`, `steps_per_frame_budget()`.

   Without a panel the clock starts paused. Call `play()` somewhere, e.g. after the first
   graph run.

### Interpolation

The clock sets `alpha` every real-time frame, just before the paint. To use it:
- keep the state from before each real-time chunk (one buffer copy at the start of
  `submit_chunk`; skip it on a reset chunk, there is nothing to blend from);
- hand both buffers and `alpha` to the renderer, which draws `mix(previous, current, alpha)`;
- when there is no previous state (after a reset, in Accuracy), bind the current buffer in its
  place and use alpha = 1.

Without this, a frame whose chunk had 0 steps (slow speeds, where a step is longer than a frame)
shows the same picture as the last one, so the motion stutters.

## Pitfalls

| Pitfall | What to do |
|---|---|
| The node is destroyed (graph replaced) while a chunk is in flight. | Never put `this` in a GPU callback. Use a `shared_ptr<Self*>` token that the destructor nulls. |
| Callbacks of an earlier run arrive during the next one. | Tag each chunk with a run serial and ignore stale ones. A real-time run is finished when its last chunk is *submitted*, so its callbacks routinely arrive late. |
| Real time queues more work than the GPU can finish. | Refuse `advance_frame()` (return false) beyond a few chunks in flight; MPM allows 3. The simulation then falls behind instead of the frame rate dropping. |
| `simulated_time()` advances on completion. | The clock computes the next frame's steps from it immediately: advance it at submission. |
| Blocking readbacks in `advance_frame()`. | Never. It runs inside the frame; everything must be asynchronous. |
| `gpu_ms_per_step()` stays 0. | The budget then falls back to the manual counts (`manual_steps_per_submit`, `manual_steps_per_run`). Measure with timestamp queries if you can. Use the *best* recent cost: small per-frame chunks measure slower per step because the GPU clocks down. |
| A run never closes. | Closing on `end_run` is mandatory. A pause or a mode switch sends a 0-step chunk with `end_run = true`, and refusing it leaves `is_advancing()` true. |
| A downstream node holds a pointer into your buffers across a run. | Reallocating on reset frees the old buffers. Let consumers re-read through the socket every frame (see the overlay crash fix: `TextureOverlay::link_texture_source`). |

## Testing

- `SimulationClock` is unit tested without a GPU (`unittests_webgpu_compute`, native builds
  only). Its `FakeSimulation` shows the minimal behaviour the clock expects. Copy it to test a
  new simulation's state machine.
- In the app, check that:
  - 1× real time holds `measured_speed()` ≈ 1.00 at 60 fps;
  - 0.1× still moves every frame (interpolation);
  - pause, step and reset work in both modes;
  - replacing the graph while playing does not crash.

## Further reading

- `mpm-mls-doc/13-simulation-driver.md`: the design decisions, the clock's real-time maths,
  and measurements with the MPM solver.
- Glenn Fiedler, "Fix Your Timestep!": the fixed-step loop with interpolation the real-time
  mode follows.

/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <cstdint>

namespace webgpu_compute {

class TimeSteppedSimulation;

/// Decides, once per frame, how far a TimeSteppedSimulation advances. Two modes:
///
/// - Accuracy: as fast as the GPU budget allows. A run of steps starts whenever the previous
///   one is done, so simulated time runs at whatever rate the GPU manages - the fastest way
///   to a result, for the AvaFrame sandbox and batch runs.
/// - RealTime: simulated time follows the wall clock times `speed`, like a game engine's
///   fixed-step loop. Wall time per frame adds to a debt; a run pays off whole time steps of
///   it. The time step never changes (explicit solver, CFL bound). A run spans
///   REAL_TIME_RUN_FRAMES frames and is submitted one chunk per frame (Submission::PerFrame),
///   its steps spread evenly, so every frame advances by about the same simulated time. When
///   the GPU cannot keep up within the budget, the debt beyond one more run is dropped: the
///   simulation falls behind (measured_speed() drops, behind() is true) instead of piling up
///   work.
///
/// No interpolation between steps: a frame shows the state after its chunk, so with steps
/// longer than a frame (low speeds) some frames show no change.
/// Plain C++ without GPU or Qt, so it is unit tested with a fake simulation.
class SimulationClock {
public:
    enum class Mode : int { RealTime = 0, Accuracy };

    struct Settings {
        Mode mode = Mode::RealTime;
        float speed = 1.0f; // RealTime: simulated seconds per wall-clock second
        /* GPU milliseconds per frame the simulation may take. Sizes the chunks from the measured
         * GPU time per step. 0 = manual: the two step counts below are used as they are. */
        float budget_ms = 6.0f;
        uint32_t manual_steps_per_submit = 2u;
        uint32_t manual_steps_per_run = 24u; // Accuracy only; RealTime runs what the debt asks for
    };

    static constexpr float MIN_SPEED = 0.1f;
    static constexpr float MAX_SPEED = 4.0f;
    static constexpr uint32_t MAX_STEPS_PER_SUBMIT = 64u;
    /// Accuracy runs are at least this long, so the diagnostics update several times a second.
    static constexpr uint32_t MIN_STEPS_PER_RUN = 24u;
    /// A longer frame (a stall, a dragged window) counts as this long.
    static constexpr float MAX_FRAME_SECONDS = 0.25f;
    /// RealTime runs span this many frames, one chunk per frame. Longer runs spread the cost
    /// per run (rasterising the overlay, the diagnostics readback) over more steps; the
    /// overlay and diagnostics update once per run.
    static constexpr uint32_t REAL_TIME_RUN_FRAMES = 8u;
    /// RealTime counts as behind below this fraction of the target speed, and as caught up
    /// again above the second.
    static constexpr float BEHIND_FRACTION = 0.9f;
    static constexpr float CAUGHT_UP_FRACTION = 0.95f;

    const Settings& settings() const { return m_settings; }
    void set_settings(const Settings& settings) { m_settings = settings; }

    bool playing() const { return m_playing; }
    void play();
    void pause();

    /// Once per frame, `wall_seconds` after the previous call. While playing, starts the
    /// next run when the simulation is idle. Pauses when the simulation cannot advance. While
    /// paused, a real-time run that is under way still finishes, one chunk per frame.
    void tick(TimeSteppedSimulation& simulation, float wall_seconds);
    /// One accuracy-sized run now, playing or not (a "Step" button). False when the
    /// simulation is busy or not ready.
    bool step(TimeSteppedSimulation& simulation);

    /// Simulated seconds per wall-clock second over the last ~1 s window while playing, 0
    /// before the first window.
    float measured_speed() const { return m_measured_speed; }
    /// RealTime playback that does not reach its speed (GPU budget too small).
    bool behind() const;
    /// Steps per submitted chunk the budget allows, as last sized (Accuracy's chunk).
    uint32_t steps_per_submit() const { return m_chunk; }
    /// Steps per frame the budget allows, fractional: what RealTime can sustain on average.
    /// Against the best GPU cost per step since the last reset, not the average: small
    /// per-frame chunks measure up to ~3x slower per step than the large chunks of Accuracy
    /// (on an M5 the GPU clocks down when it has little to do), and budgeting with those would
    /// cut the load further, down to a fraction of real time. The budget is the guard against
    /// catching up without end; real overload shows as GPU backpressure (the simulation skips
    /// a frame's chunk). The chunk size until the cost is measured.
    float steps_per_frame_budget() const;

private:
    /// Steps per chunk from the budget and the measured GPU time per step, with hysteresis:
    /// the timing jitters by a few percent, and every change moves the frame time.
    uint32_t size_chunk(const TimeSteppedSimulation& simulation);
    uint32_t accuracy_run_steps(uint32_t chunk) const;
    void start_accuracy_run(TimeSteppedSimulation& simulation, uint32_t chunk);
    void track_best_cost(const TimeSteppedSimulation& simulation);
    void measure(const TimeSteppedSimulation& simulation, float wall_seconds);

    Settings m_settings;
    bool m_playing = false;
    uint32_t m_chunk = 0; // 0 = not sized yet
    double m_debt = 0.0; // RealTime: simulated seconds owed [s]
    float m_frame_seconds = 0.0f; // smoothed wall time per frame [s]
    float m_best_ms_per_step = 0.0f; // RealTime budget: lowest GPU cost per step since the reset; 0 = none

    float m_window_wall = 0.0f; // [s]
    float m_window_start = -1.0f; // simulated time at the window start; < 0 = no window
    float m_measured_speed = 0.0f;
    bool m_behind = false;
};

} // namespace webgpu_compute

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

#include "SimulationClock.h"
#include "TimeSteppedSimulation.h"

#include <algorithm>
#include <cmath>

namespace webgpu_compute {

void SimulationClock::play()
{
    if (m_playing)
        return;
    m_playing = true;
    m_debt = 0.0;
    m_measured_speed = 0.0f;
    m_behind = false;
    m_window_start = -1.0f;
}

void SimulationClock::pause()
{
    m_playing = false;
    m_window_start = -1.0f; // a pause must not count as slow simulation
}

bool SimulationClock::behind() const { return m_playing && m_settings.mode == Mode::RealTime && m_behind; }

uint32_t SimulationClock::size_chunk(const TimeSteppedSimulation& simulation)
{
    if (m_settings.budget_ms <= 0.0f) {
        m_chunk = std::clamp(m_settings.manual_steps_per_submit, 1u, 4096u);
        return m_chunk;
    }
    const float per_step = simulation.gpu_ms_per_step();
    if (per_step <= 0.0f) {
        // Not timed yet: the manual value until the first chunk has been measured.
        if (m_chunk == 0)
            m_chunk = std::clamp(m_settings.manual_steps_per_submit, 1u, MAX_STEPS_PER_SUBMIT);
        return m_chunk;
    }
    // The budget is per chunk, and it is also per frame: the simulation submits the next chunk
    // from a chunk's work-done callback, so in steady state one chunk completes per frame (see
    // MpmSolverNode::run_impl).
    const uint32_t target = std::clamp(uint32_t(m_settings.budget_ms / per_step), 1u, MAX_STEPS_PER_SUBMIT);
    const uint32_t current = std::max(m_chunk, 1u);
    const uint32_t tolerance = std::max(1u, current / 8u);
    if (m_chunk == 0 || target + tolerance <= current || target >= current + tolerance)
        m_chunk = target;
    return m_chunk;
}

uint32_t SimulationClock::accuracy_run_steps(uint32_t chunk) const
{
    if (m_settings.budget_ms <= 0.0f)
        return std::clamp(m_settings.manual_steps_per_run, 1u, 4096u);
    // At least two chunks per run: the simulation keeps two chunks in flight, and a run that is
    // a single chunk leaves the GPU idle until the next frame starts the next run.
    return std::max(2u * chunk, MIN_STEPS_PER_RUN);
}

void SimulationClock::measure(const TimeSteppedSimulation& simulation, float wall_seconds)
{
    const float time = simulation.simulated_time();
    if (m_window_start < 0.0f || time < m_window_start) {
        // First frame, or a reset: start over. The debt belongs to the old run, the cost to the
        // old flow (or the old Detail).
        if (m_window_start >= 0.0f) {
            m_debt = 0.0;
            m_best_ms_per_step = 0.0f;
        }
        m_window_start = time;
        m_window_wall = 0.0f;
        return;
    }
    m_window_wall += wall_seconds;
    if (m_window_wall >= 1.0f) {
        m_measured_speed = (time - m_window_start) / m_window_wall;
        // With hysteresis: a run that just keeps up jitters by a few percent between windows.
        const float target = std::clamp(m_settings.speed, MIN_SPEED, MAX_SPEED);
        if (m_measured_speed < BEHIND_FRACTION * target)
            m_behind = true;
        else if (m_measured_speed > CAUGHT_UP_FRACTION * target)
            m_behind = false;
        m_window_start = time;
        m_window_wall = 0.0f;
    }
}

void SimulationClock::tick(TimeSteppedSimulation& simulation, float wall_seconds)
{
    if (!m_playing) {
        // A PerFrame run started before the pause still goes out to its end (at most
        // REAL_TIME_RUN_FRAMES - 1 more chunks): until then the simulation is busy, and a Step,
        // a Reset or a graph run would wait behind it. A no-op for chained runs.
        if (simulation.is_advancing())
            simulation.submit_frame();
        return;
    }
    if (!simulation.can_advance()) {
        pause();
        return;
    }
    const float wall = std::clamp(wall_seconds, 0.0f, MAX_FRAME_SECONDS);
    measure(simulation, wall);
    const uint32_t chunk = size_chunk(simulation);

    if (m_settings.mode == Mode::Accuracy) {
        m_debt = 0.0;
        // is_advancing() keeps runs from piling up when the GPU falls behind the frame rate.
        if (!simulation.is_advancing())
            start_accuracy_run(simulation, chunk);
        return;
    }

    const double dt = simulation.time_step();
    if (!(dt > 0.0))
        return;
    const float speed = std::clamp(m_settings.speed, MIN_SPEED, MAX_SPEED);
    m_frame_seconds = m_frame_seconds <= 0.0f ? wall : 0.9f * m_frame_seconds + 0.1f * wall;
    const double frame_time = double(speed) * double(m_frame_seconds); // simulated time per frame [s]
    // A run spans REAL_TIME_RUN_FRAMES frames with one chunk each, its steps at most what the
    // budget allows over those frames (fractional per frame, or 3.5 steps per frame would only
    // ever run 3). More debt than one more run is dropped: the simulation slows down instead
    // of spiralling into ever longer runs.
    track_best_cost(simulation);
    const uint32_t max_run = std::max(1u, uint32_t(float(REAL_TIME_RUN_FRAMES) * steps_per_frame_budget()));
    const double max_debt = double(max_run) * dt;
    m_debt += double(wall) * double(speed);
    if (simulation.is_advancing()) {
        m_debt = std::min(m_debt, 2.0 * max_debt);
        simulation.submit_frame();
        return;
    }
    // The run pays the debt and the frames it is about to span in advance: its chunks go out
    // one per frame, so by its end the wall clock has caught up with the simulated time.
    const double ahead = double(REAL_TIME_RUN_FRAMES - 1u) * frame_time;
    const uint32_t steps = uint32_t(std::clamp(std::floor((m_debt + ahead) / dt), 0.0, double(max_run)));
    if (steps == 0)
        return;
    m_debt = std::min(m_debt - double(steps) * dt, max_debt);
    simulation.advance(steps, std::min(steps, REAL_TIME_RUN_FRAMES), TimeSteppedSimulation::Submission::PerFrame);
}

void SimulationClock::start_accuracy_run(TimeSteppedSimulation& simulation, uint32_t chunk)
{
    const uint32_t steps = accuracy_run_steps(chunk);
    const uint32_t per_submit = std::min(chunk, steps);
    simulation.advance(steps, (steps + per_submit - 1u) / per_submit, TimeSteppedSimulation::Submission::Chained);
}

float SimulationClock::steps_per_frame_budget() const
{
    if (m_settings.budget_ms <= 0.0f || m_best_ms_per_step <= 0.0f)
        return float(std::max(m_chunk, 1u));
    return std::clamp(m_settings.budget_ms / m_best_ms_per_step, 1.0f, float(MAX_STEPS_PER_SUBMIT));
}

void SimulationClock::track_best_cost(const TimeSteppedSimulation& simulation)
{
    const float per_step = simulation.gpu_ms_per_step();
    if (per_step > 0.0f && (m_best_ms_per_step <= 0.0f || per_step < m_best_ms_per_step))
        m_best_ms_per_step = per_step;
}

bool SimulationClock::step(TimeSteppedSimulation& simulation)
{
    if (!simulation.can_advance() || simulation.is_advancing())
        return false;
    start_accuracy_run(simulation, size_chunk(simulation));
    return true;
}

} // namespace webgpu_compute

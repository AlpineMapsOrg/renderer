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
        // First frame, or a reset: start over. The cost belongs to the old flow (or the old Detail).
        if (m_window_start >= 0.0f)
            m_best_ms_per_step = 0.0f;
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

void SimulationClock::close_frame_run(TimeSteppedSimulation& simulation)
{
    if (simulation.advance_frame(0u, true)) {
        m_frame_run_open = false;
        m_frame_run_chunks = 0;
    }
}

bool SimulationClock::tick(TimeSteppedSimulation& simulation, float wall_seconds)
{
    if (!m_playing) {
        // A real-time run left open by the pause is closed, so a Step, a Reset or a graph run
        // does not wait behind it.
        if (m_frame_run_open)
            close_frame_run(simulation);
        return simulation.is_advancing();
    }
    if (!simulation.can_advance()) {
        pause();
        return false;
    }
    const float wall = std::clamp(wall_seconds, 0.0f, MAX_FRAME_SECONDS);
    measure(simulation, wall);
    const uint32_t chunk = size_chunk(simulation);

    if (m_settings.mode == Mode::Accuracy) {
        if (m_frame_run_open) {
            close_frame_run(simulation); // switched from real time
            return true;
        }
        simulation.set_interpolation(1.0f);
        // is_advancing() keeps runs from piling up when the GPU falls behind the frame rate.
        if (!simulation.is_advancing())
            start_accuracy_run(simulation, chunk);
        return true;
    }
    real_time_tick(simulation, wall);
    return true;
}

void SimulationClock::real_time_tick(TimeSteppedSimulation& simulation, float wall_seconds)
{
    const double dt = simulation.time_step();
    if (!(dt > 0.0))
        return;
    const double now = simulation.simulated_time();
    // Something other than our chunks moved the simulation (the first frame, a Step, a reset
    // run): present from where it is.
    if (!m_frame_run_open && (m_present < 0.0 || now != m_chunk_end))
        m_present = m_chunk_start = m_chunk_end = now;
    if (simulation.is_advancing() && !m_frame_run_open)
        return; // an accuracy run (a Step) is under way

    track_best_cost(simulation);
    const float budget = steps_per_frame_budget();
    m_budget_credit = std::min(m_budget_credit + budget, budget + 1.0f);
    const float speed = std::clamp(m_settings.speed, MIN_SPEED, MAX_SPEED);
    m_present += double(wall_seconds) * double(speed);

    // Just enough steps to reach the presentation time, as far as the budget allows.
    const double needed = (m_present - now) / dt;
    uint32_t steps = needed > 1e-6 ? uint32_t(std::ceil(needed - 1e-6)) : 0u;
    steps = std::min(steps, uint32_t(std::max(m_budget_credit, 0.0f)));
    if (steps > 0) {
        const bool end_run = m_frame_run_chunks + 1u >= REAL_TIME_RUN_CHUNKS;
        if (simulation.advance_frame(steps, end_run)) {
            m_budget_credit -= float(steps);
            const double after = simulation.simulated_time();
            if (after < now) // the run began with a reset
                m_present = m_chunk_start = after - double(steps) * dt;
            else
                m_chunk_start = now;
            m_chunk_end = after;
            m_frame_run_open = !end_run;
            m_frame_run_chunks = end_run ? 0u : m_frame_run_chunks + 1u;
        }
    }
    // Time the budget could not simulate is dropped, not caught up later: the presentation runs
    // at most one step ahead of the simulation.
    m_present = std::min(m_present, double(simulation.simulated_time()) + dt);

    float alpha = 1.0f;
    if (m_settings.interpolate && m_chunk_end > m_chunk_start)
        alpha = float(std::clamp((std::min(m_present, m_chunk_end) - m_chunk_start) / (m_chunk_end - m_chunk_start), 0.0, 1.0));
    simulation.set_interpolation(alpha);
}

void SimulationClock::start_accuracy_run(TimeSteppedSimulation& simulation, uint32_t chunk)
{
    const uint32_t steps = accuracy_run_steps(chunk);
    const uint32_t per_submit = std::min(chunk, steps);
    simulation.advance(steps, (steps + per_submit - 1u) / per_submit);
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
    simulation.set_interpolation(1.0f);
    start_accuracy_run(simulation, size_chunk(simulation));
    return true;
}

} // namespace webgpu_compute

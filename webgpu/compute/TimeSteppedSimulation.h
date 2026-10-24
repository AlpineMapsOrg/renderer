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

#include "SimulationClock.h"

#include <cstdint>

namespace webgpu_compute {

/// A simulation that advances in fixed time steps, asynchronously on the GPU, and is played
/// back frame by frame - as opposed to a node that computes its result once per graph run.
///
/// The interface is what SimulationDriver needs and nothing GPU or Qt specific: a node
/// implements it next to Node, and the driver finds it in the graph with a dynamic_cast.
/// Explicit solvers cannot stretch their time step to the frame time (the CFL bound), so the
/// only knob is how many steps to run; the clock decides that.
class TimeSteppedSimulation {
public:
    virtual ~TimeSteppedSimulation() = default;

    /// How the chunks of a run reach the GPU.
    enum class Submission {
        /* At the simulation's own pace, as fast as the GPU takes them: in MpmSolverNode two in
         * flight, the next one from the previous one's work-done callback. Done when the last
         * chunk has finished. */
        Chained,
        /* One per frame, from submit_frame(), so every rendered frame advances by one chunk.
         * Done as soon as the last chunk is submitted, so the next run can follow in the very
         * next frame without a gap. */
        PerFrame,
    };

    /// The inputs are in place, so advance() can start.
    virtual bool can_advance() const = 0;
    /// A previous advance() has not finished yet. The clock never starts a second one.
    virtual bool is_advancing() const = 0;
    /// Starts a run of `steps` time steps in `chunks` GPU submissions, the steps spread evenly
    /// over the chunks (they differ by at most one). Returns immediately. A PerFrame run
    /// submits its first chunk right away.
    virtual void advance(uint32_t steps, uint32_t chunks, Submission submission) = 0;
    /// PerFrame runs: submits the next chunk, unless the GPU is too far behind (then the frame
    /// gets none). The clock calls it once per frame while is_advancing(), paused or not.
    /// Does nothing for a chained run.
    virtual void submit_frame() = 0;

    /// The fixed time step [s] the next advance() uses.
    virtual float time_step() const = 0;
    /// Simulated time since the last reset [s]. Goes back to 0 on a reset.
    virtual float simulated_time() const = 0;
    /// Measured GPU time per step [ms], or 0 until the simulation has timed one.
    virtual float gpu_ms_per_step() const = 0;
    /// The simulation has come to rest, by its own criterion. Never, if it has none.
    virtual bool at_rest() const { return false; }
    /// Back to the initial state; takes effect with the next advance().
    virtual void reset() = 0;

    /// Playback state of this simulation. It lives with the simulation so it is gone with it
    /// when the graph is replaced, and a new node starts paused.
    SimulationClock& clock() { return m_clock; }
    const SimulationClock& clock() const { return m_clock; }

private:
    SimulationClock m_clock;
};

} // namespace webgpu_compute

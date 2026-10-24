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
///
/// Two ways to advance, one per playback mode of the clock:
/// - advance(): a run of steps at the simulation's own pace (accuracy).
/// - advance_frame(): one chunk now, so every frame gets one (real time). The chunks form runs
///   too, closed every few frames, so per-run work (overlays, diagnostics) still happens.
class TimeSteppedSimulation {
public:
    virtual ~TimeSteppedSimulation() = default;

    /// The inputs are in place, so advance() can start.
    virtual bool can_advance() const = 0;
    /// A run is under way: an advance() that has not finished, or a run of advance_frame()
    /// chunks that is still open. The clock never starts a second one.
    virtual bool is_advancing() const = 0;
    /// Accuracy: starts a run of `steps` time steps in `chunks` GPU submissions, the steps
    /// spread evenly over them (they differ by at most one), chained at the simulation's own
    /// pace (in MpmSolverNode two in flight, the next from the previous one's work-done
    /// callback). Returns immediately; done when is_advancing() is false again.
    virtual void advance(uint32_t steps, uint32_t chunks) = 0;
    /// Real time: submits one chunk of `steps` time steps now, opening a run if none is open.
    /// `end_run` closes the run with this chunk (0 steps are allowed for that). Returns false,
    /// and submits nothing, while the GPU is too far behind or an advance() run is under way.
    virtual bool advance_frame(uint32_t steps, bool end_run) = 0;
    /// Real time: what the next rendered frame should show, as the share of the way from the
    /// state before the last advance_frame() chunk to the state after it. 1 = the latest
    /// state. A simulation whose renderer cannot blend ignores it.
    virtual void set_interpolation(float) { }

    /// The fixed time step [s] the next advance() uses.
    virtual float time_step() const = 0;
    /// Simulated time since the last reset [s]. Goes back to 0 on a reset.
    virtual float simulated_time() const = 0;
    /// Measured GPU time per step [ms], or 0 until the simulation has timed one.
    virtual float gpu_ms_per_step() const = 0;
    /// The simulation has come to rest, by its own criterion. Never, if it has none.
    virtual bool at_rest() const { return false; }
    /// Back to the initial state; takes effect with the next run.
    virtual void reset() = 0;

    /// Playback state of this simulation. It lives with the simulation so it is gone with it
    /// when the graph is replaced, and a new node starts paused.
    SimulationClock& clock() { return m_clock; }
    const SimulationClock& clock() const { return m_clock; }

private:
    SimulationClock m_clock;
};

} // namespace webgpu_compute

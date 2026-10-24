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

#include "TimeSteppedSimulation.h"

#include <chrono>
#include <optional>
#include <vector>

namespace webgpu_compute::nodes {
class NodeGraph;
}

namespace webgpu_compute {

/// Plays back every TimeSteppedSimulation in a node graph: once per frame it measures the
/// wall time since the previous frame and ticks each simulation's clock with it. The app
/// calls it from its frame loop, independent of which panels are open or whether the GUI is
/// shown; panels only change the clocks (play, pause, mode, speed).
class SimulationDriver {
public:
    /// The nodes of the graph that implement TimeSteppedSimulation.
    static std::vector<TimeSteppedSimulation*> simulations(nodes::NodeGraph& graph);

    /// Once per frame. A null graph (none loaded yet) only keeps the frame time current.
    void tick(nodes::NodeGraph* graph);
    /// The same with an explicit frame time [s].
    void tick(nodes::NodeGraph* graph, float wall_seconds);

private:
    std::optional<std::chrono::steady_clock::time_point> m_last_tick;
};

} // namespace webgpu_compute

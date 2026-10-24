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

#include "SimulationDriver.h"
#include "NodeGraph.h"

namespace webgpu_compute {

std::vector<TimeSteppedSimulation*> SimulationDriver::simulations(nodes::NodeGraph& graph)
{
    std::vector<TimeSteppedSimulation*> found;
    for (auto& [name, node] : graph.get_nodes()) {
        if (auto* simulation = dynamic_cast<TimeSteppedSimulation*>(node.get()))
            found.push_back(simulation);
    }
    return found;
}

void SimulationDriver::tick(nodes::NodeGraph* graph)
{
    const auto now = std::chrono::steady_clock::now();
    const float wall_seconds = m_last_tick ? std::chrono::duration<float>(now - *m_last_tick).count() : 0.0f;
    m_last_tick = now;
    tick(graph, wall_seconds);
}

void SimulationDriver::tick(nodes::NodeGraph* graph, float wall_seconds)
{
    if (graph == nullptr)
        return;
    for (TimeSteppedSimulation* simulation : simulations(*graph))
        simulation->clock().tick(*simulation, wall_seconds);
}

} // namespace webgpu_compute

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

#include "ui/ImGuiPanel.h"

namespace webgpu_compute::nodes {
class MpmSolverNode;
}

namespace webgpu_app {

class NodeGraphPanel;

/// Sidebar controls for the MLS-MPM avalanche solver.
///
/// The solver is a compute node, but stepping it from the node editor's settings panel
/// means the animation only advances while that editor is open and the node is selected.
/// This panel owns the transport instead and advances the simulation from draw(), which
/// runs every frame regardless of which panels are open or collapsed.
class AvalanchePanel : public ImGuiPanel {
    Q_OBJECT

public:
    explicit AvalanchePanel(NodeGraphPanel* graph_panel);

    // Advances the simulation. Runs every frame, independent of any panel visibility.
    void draw() override;
    void draw_panel() override;

private:
    /// Looks up the solver in the currently loaded graph. Returns nullptr if the active
    /// graph has no MPM node. Re-resolved every frame because loading a preset replaces
    /// the graph and every node in it.
    webgpu_compute::nodes::MpmSolverNode* find_solver() const;

private:
    NodeGraphPanel* m_graph_panel;
    bool m_playing = false;
};

} // namespace webgpu_app

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

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace webgpu_compute::nodes {
class MpmSolverNode;
class GeoRegionNode;
class SelectTilesNode;
class Node;
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
    /// A place the user can pick, expressed geographically so it does not depend on the
    /// tiles the graph happens to load.
    struct Scenario {
        std::string name;
        std::string note; // one-line description shown under the picker

        glm::dvec2 region_center; // latitude, longitude
        float region_extent; // terrain to load around the centre [m]
        uint32_t zoomlevel; // tile zoom; lower means larger area, coarser DEM

        glm::dvec2 domain_center; // latitude, longitude
        float domain_size; // [m]
        uint32_t grid_resolution;

        glm::dvec2 release_center; // latitude, longitude
        float release_radius; // [m]
        float slab_thickness; // [m]
    };

    /// A named material parameter set. Presets are a UI concept layered on top of the
    /// solver's ordinary settings: applying one *writes* those settings, nothing more. That
    /// keeps a single source of truth and lets a preset be edited afterwards.
    ///
    /// Values for the four flow regimes are Li et al. 2021, Table 1 (Cases I-IV) and their
    /// verification case (V); Stomakhin 2013 Table 1 for the film-snow default.
    struct MaterialPreset {
        std::string name;
        std::string note; // one line, shown under the picker

        uint32_t model; // webgpu_compute::nodes::MpmSolverNode::ConstitutiveModel

        // shared by every model
        float youngs_modulus;
        float poissons_ratio;
        float snow_density;
        float terrain_friction; // basal mu

        // Stomakhin 2013
        float hardening;
        float critical_compression;
        float critical_stretch;

        // Drucker-Prager
        float dp_friction_angle;

        // Cohesive Cam Clay
        float ccc_m;
        float ccc_beta;
        float ccc_xi;
        float ccc_p0_initial;
    };

    explicit AvalanchePanel(NodeGraphPanel* graph_panel);

    // Advances the simulation. Runs every frame, independent of any panel visibility.
    void draw() override;
    void draw_panel() override;

private:
    /// Looks up the solver in the currently loaded graph. Returns nullptr if the active
    /// graph has no MPM node. Re-resolved every frame because loading a preset replaces
    /// the graph and every node in it.
    webgpu_compute::nodes::MpmSolverNode* find_solver() const;

    /// Finds the first node of the given type in the active graph, or nullptr.
    template <typename T> T* find_node() const;

    /// Writes the scenario into the region, tile and solver nodes and re-runs the graph.
    /// Returns false when the active graph cannot express it (e.g. no GeoRegionNode).
    bool apply_scenario(const Scenario& scenario);

    /// Writes a material preset into the solver settings, forces a reseed (the plastic state
    /// is model-specific) and pulls dt under the CFL bound of the new stiffness.
    void apply_material_preset(const MaterialPreset& preset);

private:
    NodeGraphPanel* m_graph_panel;
    bool m_playing = false;

    std::vector<Scenario> m_scenarios;
    int m_selected_scenario = 0;
    std::string m_scenario_error;

    std::vector<MaterialPreset> m_material_presets;
    int m_selected_material_preset = 0; // 0 = custom (no preset active)
};

} // namespace webgpu_app

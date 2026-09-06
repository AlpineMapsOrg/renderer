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

#include "AvalanchePanel.h"

#include "compute/NodeGraphPanel.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <algorithm>
#include <nucleus/srs.h>
#include <webgpu/compute/NodeGraph.h>
#include <webgpu/compute/nodes/GeoRegionNode.h>
#include <webgpu/compute/nodes/MpmSolverNode.h>
#include <webgpu/compute/nodes/SelectTilesNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

AvalanchePanel::AvalanchePanel(NodeGraphPanel* graph_panel)
    : m_graph_panel(graph_panel)
    , m_scenarios({
          Scenario { "Breite Ries (Schneeberg)", "Gully on Austria's easternmost 2000er, 1931 m to 1364 m.",
              glm::dvec2(47.77663, 15.81600), 2500.0f, 15, glm::dvec2(47.77663, 15.81600), 1600.0f, 128, glm::dvec2(47.77480, 15.81050), 100.0f, 1.5f },
          Scenario { "Grossglockner (Pasterze side)", "Austria's highest summit, 3798 m, above the Pasterze glacier.",
              glm::dvec2(47.07900, 12.70200), 4500.0f, 15, glm::dvec2(47.07900, 12.70200), 2600.0f, 128, glm::dvec2(47.07500, 12.69600), 150.0f, 2.0f },
          Scenario { "Dachstein (Hallstatt Glacier)", "North side of the Dachstein plateau above the glacier.",
              glm::dvec2(47.47800, 13.60600), 3500.0f, 15, glm::dvec2(47.47800, 13.60600), 2200.0f, 128, glm::dvec2(47.47300, 13.60400), 130.0f, 2.0f },
      })
{
}

template <typename T> T* AvalanchePanel::find_node() const
{
    auto* graph = m_graph_panel->node_graph();
    if (graph == nullptr)
        return nullptr;
    for (auto& [name, node] : graph->get_nodes()) {
        if (auto* typed = dynamic_cast<T*>(node.get()))
            return typed;
    }
    return nullptr;
}

nodes::MpmSolverNode* AvalanchePanel::find_solver() const { return find_node<nodes::MpmSolverNode>(); }

bool AvalanchePanel::apply_scenario(const Scenario& scenario)
{
    auto* graph = m_graph_panel->node_graph();
    auto* region = find_node<nodes::GeoRegionNode>();
    auto* solver = find_solver();
    if (graph == nullptr || region == nullptr || solver == nullptr)
        return false;

    auto region_settings = region->get_settings();
    region_settings.center_latitude = scenario.region_center.x;
    region_settings.center_longitude = scenario.region_center.y;
    region_settings.extent = scenario.region_extent;
    region->set_settings(region_settings);

    if (auto* tiles = find_node<nodes::SelectTilesNode>()) {
        auto tile_settings = tiles->get_settings();
        tile_settings.zoomlevel = scenario.zoomlevel;
        tiles->set_settings(tile_settings);
    }

    auto solver_settings = solver->get_settings();
    solver_settings.domain_center = scenario.domain_center;
    solver_settings.domain_size_xy = scenario.domain_size;
    solver_settings.grid_resolution_xy = scenario.grid_resolution;
    solver_settings.grid_resolution_z = scenario.grid_resolution;
    solver_settings.release_center = scenario.release_center;
    solver_settings.release_radius = scenario.release_radius;
    solver_settings.slab_thickness = scenario.slab_thickness;
    solver_settings.reset_on_next_run = true;
    solver->set_settings(solver_settings);

    m_playing = false; // the terrain has to be fetched and stitched before stepping again
    graph->run();
    return true;
}

void AvalanchePanel::draw()
{
    // Stepping lives here rather than in draw_panel() so the animation keeps running with
    // the sidebar section collapsed and the node graph editor closed.
    if (!m_playing)
        return;

    auto* solver = find_solver();
    if (solver == nullptr || !solver->has_valid_inputs()) {
        m_playing = false;
        return;
    }
    // is_running() keeps runs from piling up when the GPU falls behind the frame rate.
    if (!solver->is_running())
        solver->rerun();
}

void AvalanchePanel::draw_panel()
{
    auto* solver = find_solver();
    if (solver == nullptr)
        return; // active graph has no avalanche simulation

    if (!ImGui::CollapsingHeader(ICON_FA_SNOWFLAKE "  Avalanche", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // --- Location ---
    // Needs a GeoRegionNode in the graph; a GPX-driven graph has no coordinates to set.
    const bool can_switch_location = find_node<nodes::GeoRegionNode>() != nullptr;
    ImGui::BeginDisabled(!can_switch_location);
    std::vector<const char*> names;
    names.reserve(m_scenarios.size());
    for (const auto& scenario : m_scenarios)
        names.push_back(scenario.name.c_str());
    if (ImGui::Combo("Location", &m_selected_scenario, names.data(), int(names.size()))) {
        m_scenario_error = apply_scenario(m_scenarios[size_t(m_selected_scenario)])
            ? std::string {}
            : std::string { "Could not apply - the active graph has no GeoRegionNode." };
    }
    ImGui::EndDisabled();

    if (!can_switch_location) {
        ImGui::TextDisabled("Load the MLS-MPM preset to switch location.");
    } else {
        ImGui::TextDisabled("%s", m_scenarios[size_t(m_selected_scenario)].note.c_str());
    }
    if (!m_scenario_error.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_scenario_error.c_str());

    ImGui::Separator();

    const bool ready = solver->has_valid_inputs();
    if (!ready) {
        m_playing = false;
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Inputs not ready.");
        ImGui::TextDisabled("Run the compute graph once (Shift+R).");
        return;
    }

    auto settings = solver->get_settings();
    bool settings_changed = false;
    bool step_now = false;

    ImGui::Text("Simulated time: %.2f s", double(solver->simulated_time()));

    const auto& aabb = solver->domain_aabb();
    if (aabb.size().x > 0.0) {
        const glm::dvec2 centre = nucleus::srs::world_to_lat_long((aabb.min + aabb.max) * 0.5);
        ImGui::TextDisabled("Centre: %.5f, %.5f", centre.x, centre.y);
    }

    if (ImGui::Button(m_playing ? ICON_FA_PAUSE "  Pause" : ICON_FA_PLAY "  Play", ImVec2(110, 0)))
        m_playing = !m_playing;
    ImGui::SameLine();
    if (ImGui::Button("Step", ImVec2(80, 0)))
        step_now = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(80, 0))) {
        solver->request_reset();
        settings = solver->get_settings();
        step_now = true;
    }

    ImGui::Separator();

    const uint32_t min_substeps = 1, max_substeps = 512;
    settings_changed |= ImGui::DragScalar("Substeps per run", ImGuiDataType_U32, &settings.substeps_per_run, 1.0f, &min_substeps, &max_substeps, "%u");
    settings_changed |= ImGui::DragFloat("Time step", &settings.dt, 0.0005f, 0.0001f, 0.5f, "%.4f s");
    settings_changed |= ImGui::DragFloat("Splat radius", &settings.splat_radius, 0.25f, 0.0f, 64.0f, "%.1f m");

    // Domain changes reallocate GPU buffers, so they reseed - apply them on release only.
    ImGui::TextDisabled("Simulation domain (changes reset the run)");
    settings_changed |= ImGui::DragScalarN("Domain lat/lon", ImGuiDataType_Double, &settings.domain_center.x, 2, 0.0001f, nullptr, nullptr, "%.5f");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();
    settings_changed |= ImGui::DragFloat("Domain size", &settings.domain_size_xy, 16.0f, 64.0f, 16384.0f, "%.0f m");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();

    const uint32_t min_res = 8, max_res = 256;
    settings_changed |= ImGui::DragScalar("Grid resolution XY", ImGuiDataType_U32, &settings.grid_resolution_xy, 1.0f, &min_res, &max_res, "%u");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();
    settings_changed |= ImGui::DragScalar("Grid resolution Z", ImGuiDataType_U32, &settings.grid_resolution_z, 1.0f, &min_res, &max_res, "%u");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();

    // The domain is clamped to the terrain the graph actually stitched, so report what the
    // solver settled on rather than what was asked for.
    const float effective_domain = float(aabb.size().x);
    const float dx = effective_domain / float(std::max(settings.grid_resolution_xy, 1u));
    ImGui::TextDisabled("Effective: %.0f m box, %.1f m cells, %.0f m vertical", double(effective_domain), double(dx),
        double(dx * float(settings.grid_resolution_z)));
    if (effective_domain + 1.0f < settings.domain_size_xy) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Clamped to the tiled region.");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The domain cannot exceed the terrain the graph loaded.\n"
                              "Lower the Select Tiles zoom level for a larger region.");
    }

    ImGui::Separator();

    // Release zone is independent of the domain: small start area, large runout box.
    ImGui::TextDisabled("Release zone (changes reset the run)");
    settings_changed |= ImGui::DragScalarN("Release lat/lon", ImGuiDataType_Double, &settings.release_center.x, 2, 0.0001f, nullptr, nullptr, "%.5f");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();
    settings_changed |= ImGui::DragFloat("Release radius", &settings.release_radius, 2.0f, 1.0f, 2000.0f, "%.0f m");
    step_now |= ImGui::IsItemDeactivatedAfterEdit();

    if (ImGui::Checkbox("Seed anywhere (ignore release areas)", &settings.seed_anywhere)) {
        settings_changed = true;
        settings.reset_on_next_run = true;
        step_now = true;
    }

    ImGui::TextDisabled("Full parameters: compute graph editor.");

    if (settings_changed)
        solver->set_settings(settings);
    if (step_now && !solver->is_running())
        solver->rerun();
}

} // namespace webgpu_app

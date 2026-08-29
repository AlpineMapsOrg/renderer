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
#include <webgpu/compute/nodes/MpmSolverNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

AvalanchePanel::AvalanchePanel(NodeGraphPanel* graph_panel)
    : m_graph_panel(graph_panel)
{
}

nodes::MpmSolverNode* AvalanchePanel::find_solver() const
{
    auto* graph = m_graph_panel->node_graph();
    if (graph == nullptr)
        return nullptr;
    for (auto& [name, node] : graph->get_nodes()) {
        if (auto* solver = dynamic_cast<nodes::MpmSolverNode*>(node.get()))
            return solver;
    }
    return nullptr;
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
    settings_changed |= ImGui::SliderFloat2("Centre in region", &settings.domain_center.x, 0.0f, 1.0f, "%.3f");
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
    settings_changed |= ImGui::SliderFloat2("Release centre", &settings.release_center.x, 0.0f, 1.0f, "%.3f");
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

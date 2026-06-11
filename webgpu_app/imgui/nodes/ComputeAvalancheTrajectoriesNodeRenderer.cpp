/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "ComputeAvalancheTrajectoriesNodeRenderer.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <webgpu_compute/nodes/ComputeAvalancheTrajectoriesNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

using Node = nodes::ComputeAvalancheTrajectoriesNode;

ComputeAvalancheTrajectoriesNodeRenderer::ComputeAvalancheTrajectoriesNodeRenderer(const std::string& name, nodes::ComputeAvalancheTrajectoriesNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
{
}

void ComputeAvalancheTrajectoriesNodeRenderer::render_settings_content()
{
    auto settings = m_node->get_settings();
    bool settings_changed = false;
    bool rerun = false;

    // --- General ---
    const uint32_t min_res = 1, max_res = 32;
    settings_changed |= ImGui::SliderScalar("Output Resolution", ImGuiDataType_U32, &settings.resolution_multiplier, &min_res, &max_res, "%ux");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    const uint32_t min_steps = 1, max_steps = 20000;
    settings_changed |= ImGui::DragScalar("Num steps", ImGuiDataType_U32, &settings.num_steps, 1.0f, &min_steps, &max_steps, "%u");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    const uint32_t min_paths = 1, max_paths = 2048;
    settings_changed
        |= ImGui::DragScalar("Num particles per cell", ImGuiDataType_U32, &settings.num_paths_per_release_cell, 1.0f, &min_paths, &max_paths, "%u");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    const uint32_t min_runs = 1, max_runs = 1000;
    settings_changed |= ImGui::DragScalar("Number of Runs", ImGuiDataType_U32, &settings.num_runs, 1.0f, &min_runs, &max_runs, "%u");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    const uint32_t min_seed = 1, max_seed = 1000000;
    settings_changed |= ImGui::DragScalar("Random seed", ImGuiDataType_U32, &settings.random_seed, 1.0f, &min_seed, &max_seed);
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::Separator();

    // --- Physics model ---
    if (ImGui::Combo("Model", (int*)&settings.active_model, "weBIGeo Avalanche Simulation\0Physics Less Simple\0")) {
        settings_changed = rerun = true;
    }

    if (settings.active_model == Node::PhysicsModelType::WEBIGEO_AVALANCHE_SIMULATION) {
        float perturbation_deg = glm::degrees(settings.max_perturbation);
        if (ImGui::DragFloat("Max Perturbation", &perturbation_deg, 0.1f, 0.0f, 90.0f, "%.1f°")) {
            settings.max_perturbation = glm::radians(perturbation_deg);
            settings_changed = true;
        }
        rerun |= ImGui::IsItemDeactivatedAfterEdit();

        settings_changed |= ImGui::DragFloat("Persistence", &settings.persistence_contribution, 0.01f, 0.0f, 0.99f, "%.2f");
        rerun |= ImGui::IsItemDeactivatedAfterEdit();

        settings_changed |= ImGui::DragFloat("Alpha", &settings.runout_flowpy.alpha, 0.01f, 0.0f, 90.0f, "%.2f°");
        rerun |= ImGui::IsItemDeactivatedAfterEdit();

    } else if (settings.active_model == Node::PhysicsModelType::PHYSICS_LESS_SIMPLE) {
        settings_changed |= ImGui::SliderFloat("Gravity", &settings.model2.gravity, 0.0f, 15.0f, "%.2f");
        rerun |= ImGui::IsItemDeactivatedAfterEdit();

        settings_changed |= ImGui::SliderFloat("Mass", &settings.model2.mass, 0.0f, 100.0f, "%.2f");
        rerun |= ImGui::IsItemDeactivatedAfterEdit();

        settings_changed |= ImGui::SliderFloat("Drag coeff", &settings.model2.drag_coeff, 1.0f, 10000.0f, "%.0f");
        rerun |= ImGui::IsItemDeactivatedAfterEdit();

        settings_changed |= ImGui::SliderFloat("Friction coeff", &settings.model2.friction_coeff, 0.0f, 0.5f, "%.3f");
        rerun |= ImGui::IsItemDeactivatedAfterEdit();

        if (ImGui::Combo("Friction model", (int*)&settings.active_runout_model, "Coulomb\0Voellmy\0Voellmy Min Shear\0SamosAt\0")) {
            settings_changed = rerun = true;
        }
    }

    if (settings_changed)
        m_node->set_settings(settings);
    if (rerun)
        m_node->rerun();
}

} // namespace webgpu_app

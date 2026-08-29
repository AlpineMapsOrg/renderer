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

#include "MpmSolverNodeRenderer.h"

#include <cmath>
#include <imgui.h>
#include <webgpu/compute/nodes/MpmSolverNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

using Node = nodes::MpmSolverNode;

MpmSolverNodeRenderer::MpmSolverNodeRenderer(const std::string& name, nodes::MpmSolverNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
{
}

void MpmSolverNodeRenderer::render_settings_content()
{
    auto settings = m_node->get_settings();
    bool settings_changed = false;
    bool rerun = false;

    // --- Transport ---
    ImGui::Text("Simulated time: %.2f s", double(m_node->simulated_time()));

    if (ImGui::Button(m_playing ? "Pause" : "Play")) {
        m_playing = !m_playing;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        rerun = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        m_node->request_reset();
        settings = m_node->get_settings();
        rerun = true;
    }
    if (m_playing) {
        ImGui::TextDisabled("Playing - keep this panel open to keep stepping.");
    }

    ImGui::Separator();

    // --- Time stepping ---
    const float dx = settings.domain_size_xy / float(std::max(settings.grid_resolution_xy, 1u));
    const float wave_speed = std::sqrt(std::max(settings.youngs_modulus, 1.0f) / std::max(settings.snow_density, 1.0f));
    const float cfl_dt = 0.1f * dx / std::max(wave_speed, 1e-3f);
    ImGui::Text("Grid spacing: %.2f m", double(dx));
    ImGui::Text("CFL suggestion: dt <= %.4f s", double(cfl_dt));
    if (settings.dt > cfl_dt) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "dt above the CFL estimate - may go unstable.");
    }

    settings_changed |= ImGui::DragFloat("Time step (dt)", &settings.dt, 0.0005f, 0.0001f, 0.5f, "%.4f s");

    const uint32_t min_substeps = 1, max_substeps = 512;
    settings_changed |= ImGui::DragScalar("Substeps per run", ImGuiDataType_U32, &settings.substeps_per_run, 1.0f, &min_substeps, &max_substeps, "%u");

    ImGui::Separator();

    // --- Domain ---
    // Changing any of these reallocates GPU buffers, which forces a reseed.
    ImGui::TextDisabled("Domain (changes trigger a reset)");
    settings_changed |= ImGui::SliderFloat2("Center in region", &settings.domain_center.x, 0.0f, 1.0f, "%.3f");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    settings_changed |= ImGui::DragFloat("Domain size", &settings.domain_size_xy, 8.0f, 64.0f, 8192.0f, "%.0f m");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    const uint32_t min_res = 8, max_res = 256;
    settings_changed |= ImGui::DragScalar("Grid resolution XY", ImGuiDataType_U32, &settings.grid_resolution_xy, 1.0f, &min_res, &max_res, "%u");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    settings_changed |= ImGui::DragScalar("Grid resolution Z", ImGuiDataType_U32, &settings.grid_resolution_z, 1.0f, &min_res, &max_res, "%u");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::TextDisabled("Vertical range: %.0f m", double(dx * float(settings.grid_resolution_z)));

    ImGui::Separator();

    // --- Released snow ---
    const uint32_t min_particles = 1024, max_particles = 1u << 21;
    settings_changed |= ImGui::DragScalar("Particles", ImGuiDataType_U32, &settings.num_particles, 256.0f, &min_particles, &max_particles, "%u");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    settings_changed |= ImGui::DragFloat("Slab thickness", &settings.slab_thickness, 0.05f, 0.0f, 20.0f, "%.2f m");
    settings_changed |= ImGui::DragFloat("Snow density", &settings.snow_density, 1.0f, 50.0f, 917.0f, "%.0f kg/m3");

    const uint32_t min_seed = 1, max_seed = 1000000;
    settings_changed |= ImGui::DragScalar("Random seed", ImGuiDataType_U32, &settings.random_seed, 1.0f, &min_seed, &max_seed);

    ImGui::Separator();

    // --- Snow material (Stomakhin et al. 2013) ---
    ImGui::TextDisabled("Snow material (Stomakhin et al. 2013)");
    settings_changed |= ImGui::DragFloat("Young's modulus", &settings.youngs_modulus, 1000.0f, 1.0e3f, 1.0e7f, "%.0f Pa");
    settings_changed |= ImGui::DragFloat("Poisson's ratio", &settings.poissons_ratio, 0.005f, 0.0f, 0.45f, "%.3f");
    settings_changed |= ImGui::DragFloat("Hardening", &settings.hardening, 0.1f, 0.0f, 30.0f, "%.2f");
    settings_changed |= ImGui::DragFloat("Critical compression", &settings.critical_compression, 0.001f, 0.0f, 0.5f, "%.4f");
    settings_changed |= ImGui::DragFloat("Critical stretch", &settings.critical_stretch, 0.0005f, 0.0f, 0.5f, "%.4f");

    ImGui::Separator();

    settings_changed |= ImGui::DragFloat("Gravity", &settings.gravity, 0.05f, 0.0f, 30.0f, "%.2f m/s2");
    settings_changed |= ImGui::DragFloat("Terrain friction", &settings.terrain_friction, 0.01f, 0.0f, 2.0f, "%.2f");

    const uint32_t min_raster = 64, max_raster = 4096;
    settings_changed |= ImGui::DragScalar("Output resolution", ImGuiDataType_U32, &settings.raster_resolution, 8.0f, &min_raster, &max_raster, "%u");
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    if (settings_changed)
        m_node->set_settings(settings);

    // Drive the animation. is_running() keeps runs from piling up if the GPU falls behind
    // the UI frame rate.
    if ((rerun || m_playing) && !m_node->is_running())
        m_node->rerun();
}

} // namespace webgpu_app

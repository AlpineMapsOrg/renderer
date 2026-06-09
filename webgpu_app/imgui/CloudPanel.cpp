/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
 * Copyright (C) 2026 Wendelin Muth
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

#include "CloudPanel.h"

#include "../ImGuiManager.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>

#include "../CloudsManager.h"
#include <webgpu_engine/Context.h>
#include <webgpu_engine/cloud/CloudRenderer.h>

namespace webgpu_app {

CloudPanel::CloudPanel(webgpu_engine::Context* context, clouds::Manager* clouds_manager, webgpu_engine::CloudRenderer* cloud_renderer)
    : m_context(context)
    , m_clouds_manager(clouds_manager)
    , m_cloud_renderer(cloud_renderer)
{
}

void CloudPanel::draw()
{
    auto& cfg = m_context->shared_config();
    if (ImGuiManager::FloatingToggleButton("ToggleCloudsButton", ICON_FA_CLOUD, "Clouds", &cfg.m_clouds_enabled))
        m_context->request_redraw();
}

void CloudPanel::draw_panel()
{
    if (!m_context->shared_config().m_clouds_enabled)
        return;

    const auto& tilesets = m_clouds_manager->get_slots();
    auto selected_slot = m_clouds_manager->selected_time_slot();

    if (ImGui::CollapsingHeader(ICON_FA_CLOUD "  Clouds")) {

        ImGui::SeparatorText("Data");

        if (tilesets.empty()) {
            if (m_clouds_manager->is_loading()) {
                ImGui::Text("Loading cloud data...");
            } else {
                ImGui::Text("No cloud data available.");
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_SYNC "##reload_clouds")) {
                    m_clouds_manager->refresh_tileset_list();
                }
            }
        } else {
            std::string preview_str = "Select time";
            if (!selected_slot.id.isEmpty()) {
                preview_str = selected_slot.format_string();
            }
            if (ImGui::BeginCombo("(UTC)", preview_str.c_str())) {
                for (int n = 0; n < (int)tilesets.size(); n++) {
                    const auto& slot = tilesets[n];
                    ImGui::PushID(slot.id.toStdString().c_str());
                    const bool is_selected = slot.id == selected_slot.id;
                    std::string label = slot.format_string();
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        m_clouds_manager->select_time_slot(tilesets[n]);
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            const bool loading = m_clouds_manager->is_loading();
            if (loading)
                ImGui::BeginDisabled();
            if (ImGui::Button(ICON_FA_SYNC "##reload_clouds")) {
                m_clouds_manager->refresh_tileset_list();
            }
            if (loading)
                ImGui::EndDisabled();
        }

        ImGui::SeparatorText("Shading");
        auto& shader_params = m_cloud_renderer->shader_params;
        ImGui::Text("Step Size");
        ImGui::Indent();
        ImGui::DragFloat("Minimum", &shader_params.step_size_min, 1.0f, 0.0f, 10000.0f);
        float inv_dist_fact = 1.0f / shader_params.step_size_distance_factor;
        if (ImGui::DragFloat("Distance Factor", &inv_dist_fact, 1.0f, 0.0f, 10000.0f)) {
            shader_params.step_size_distance_factor = 1.0f / inv_dist_fact;
        }
        ImGui::DragFloat("Horizon Factor", &shader_params.step_size_horizon_factor, 1.0f, 0.0f, 10000.0f);
        ImGui::Unindent();
        ImGui::Text("Scattering");
        ImGui::Indent();
        ImGui::SliderFloat("Scattering Coeff", &shader_params.scattering_coeff, -1.0f, 1.0f);
        ImGui::SliderFloat("Extinction Coeff", &shader_params.extinction_coeff, 0.0f, 1.0f, "%.5f");
        ImGui::SliderFloat("Albedo", &shader_params.albedo, 0.0f, 1.0f);
        ImGui::Unindent();
        ImGui::Text("Lighting");
        ImGui::Indent();
        ImGui::DragFloat("Sun Light Scale", &shader_params.sun_light_scale, 1.0f, 0.0f, 10000.0f);
        ImGui::DragFloat("Ambient Light Scale", &shader_params.ambient_light_scale, 0.01f, 0.0f, 10000.0f);
        ImGui::DragFloat("Atmospheric Light Scale", &shader_params.atmospheric_light_scale, 0.01f, 0.0f, 10000.0f);
        ImGui::DragFloat("Shadow Extinction Scale", &shader_params.shadow_extinction_scale, 0.01f, 0.0f, 10000.0f);
        ImGui::SliderFloat("Powder Effect Scale", &shader_params.powder_scale, 0.0f, 1.0f);
        ImGui::Unindent();
        ImGui::Text("Visibility");
        ImGui::Indent();
        ImGui::SliderFloat("Fade", &shader_params.fade_factor, 0.001f, 1.0f);
        ImGui::Unindent();
        ImGui::Text("Accumulation");
        ImGui::Indent();
        ImGui::SliderInt("Stable Frames Limit", &shader_params.stable_frames_limit, 1, 256);
        ImGui::Unindent();
    }
}

} // namespace webgpu_app

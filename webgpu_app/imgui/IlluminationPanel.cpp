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

#include "IlluminationPanel.h"

#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <glm/glm.hpp>

#include "webgpu_engine/Context.h"

namespace webgpu_app {

IlluminationPanel::IlluminationPanel(webgpu_engine::Context* context)
    : m_context(context)
{
}

void IlluminationPanel::draw_panel()
{
    if (ImGui::CollapsingHeader(ICON_FA_SUN "  Illumination")) {
        auto& cfg = m_context->shared_config();
        bool changed = ImGui::ColorEdit3("Light Color", (float*)&cfg.m_sun_light);
        changed |= ImGui::SliderFloat("Light Intensity", &cfg.m_sun_light.w, 0.0f, 10.0f);
        changed |= ImGui::DragFloat3("Light Direction", (float*)&cfg.m_sun_light_dir, 0.01f, -1.0f, 1.0f);
        ImGui::Separator();
        changed |= ImGui::ColorEdit3("Ambient Color", (float*)&cfg.m_amb_light);
        changed |= ImGui::SliderFloat("Ambient Intensity", &cfg.m_amb_light.w, 0.0f, 10.0f);
        ImGui::Separator();
        changed |= ImGui::ColorEdit4("Material Color", (float*)&cfg.m_material_color);
        ImGui::Separator();
        changed |= ImGui::SliderFloat("Ambient Strength", &cfg.m_material_light_response.x, 0.0f, 5.0f);
        changed |= ImGui::SliderFloat("Diffuse Strength", &cfg.m_material_light_response.y, 0.0f, 5.0f);
        changed |= ImGui::SliderFloat("Specular Strength", &cfg.m_material_light_response.z, 0.0f, 5.0f);
        changed |= ImGui::SliderFloat("Shininess", &cfg.m_material_light_response.w, 1.0f, 256.0f);

        if (changed) {
            cfg.m_sun_light_dir = glm::normalize(cfg.m_sun_light_dir);
            m_context->request_redraw();
        }
    }
}

} // namespace webgpu_app

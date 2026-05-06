/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "CameraPanel.h"

#include <IconsFontAwesome5.h>
#include <imgui.h>

#include "../TerrainRenderer.h"
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/srs.h>
#include <glm/gtc/type_ptr.hpp>

namespace webgpu_app {

CameraPanel::CameraPanel(TerrainRenderer* terrain_renderer)
    : m_terrain_renderer(terrain_renderer)
{
    // Lets build a vector of std::string...
    const auto position_storage = nucleus::camera::PositionStorage::instance();
    const QList<QString> position_storage_list = position_storage->getPositionList();
    for (const auto& position : position_storage_list) {
        m_camera_preset_names.push_back(position.toStdString());
    }
}

void CameraPanel::draw_panel()
{
    if (ImGui::CollapsingHeader(ICON_FA_CAMERA " Camera")) {
        if (ImGui::BeginCombo("Preset", m_camera_preset_names[m_selected_camera_preset].c_str())) {
            for (size_t n = 0; n < m_camera_preset_names.size(); n++) {
                bool is_selected = (size_t(m_selected_camera_preset) == n);
                if (ImGui::Selectable(m_camera_preset_names[n].c_str(), is_selected)) {
                    m_selected_camera_preset = int(n);

                    const auto position_storage = nucleus::camera::PositionStorage::instance();
                    const auto camera_controller = m_terrain_renderer->get_camera_controller();
                    auto new_definition = position_storage->get_by_index(m_selected_camera_preset);
                    auto old_vp_size = camera_controller->definition().viewport_size();
                    new_definition.set_viewport_size(old_vp_size);
                    camera_controller->set_model_matrix(new_definition);
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        {
            auto& camera = m_terrain_renderer->get_camera_controller()->definition();
            auto pos = camera.position();
            glm::vec3 posf = pos;
            ImGui::InputFloat3("Position", glm::value_ptr(posf), "%.2f", ImGuiInputTextFlags_ReadOnly);
            glm::vec3 coords = nucleus::srs::world_to_lat_long_alt(pos);
            ImGui::InputFloat3("Coords", glm::value_ptr(coords), "%.6f", ImGuiInputTextFlags_ReadOnly);
            float fov = camera.field_of_view();
            if (ImGui::SliderFloat("FoV", &fov, 1.0f, 179.0f)) {
                m_terrain_renderer->get_camera_controller()->set_field_of_view(fov);
            }
        }
    }
}

} // namespace webgpu_app

/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
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

#include "ComputeSnowNodeRenderer.h"

#include <imgui.h>
#include <webgpu/compute/nodes/ComputeSnowNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

ComputeSnowNodeRenderer::ComputeSnowNodeRenderer(const std::string& name, nodes::ComputeSnowNode& node)
    : NodeRenderer(name, node)
    , m_snow_node(&node)
{
}

void ComputeSnowNodeRenderer::render_settings_content()
{
    auto settings = m_snow_node->get_settings();
    bool changed = false;

    changed
        |= ImGui::DragFloatRange2("Ang.-limit", &settings.min_angle, &settings.max_angle, 0.1f, 0.0f, 90.0f, "%.1f°", "%.1f°", ImGuiSliderFlags_AlwaysClamp);
    changed |= ImGui::SliderFloat("Ang.-blend", &settings.angle_blend, 0.0f, 90.0f, "%.1f°");
    changed |= ImGui::SliderFloat("Alt.-limit", &settings.min_altitude, 0.0f, 4000.0f, "%.1fm");
    changed |= ImGui::SliderFloat("Alt.-variation", &settings.altitude_variation, 0.0f, 1000.0f, "%.1fm");
    changed |= ImGui::SliderFloat("Alt.-blend", &settings.altitude_blend, 0.0f, 1000.0f, "%.1fm");

    if (changed) {
        m_snow_node->set_settings(settings);
        m_snow_node->rerun();
    }
}

} // namespace webgpu_app

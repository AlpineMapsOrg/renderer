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

#include "ComputeReleasePointsNodeRenderer.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <webgpu_compute/nodes/ComputeReleasePointsNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

ComputeReleasePointsNodeRenderer::ComputeReleasePointsNodeRenderer(const std::string& name, nodes::ComputeReleasePointsNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
{
}

void ComputeReleasePointsNodeRenderer::render_settings_content()
{
    auto settings = m_node->get_settings();
    bool settings_changed = false;
    bool rerun = false;

    int interval = (int)settings.sampling_interval.x;
    if (ImGui::SliderInt("Interval", &interval, 1, 64, "%u")) {
        settings.sampling_interval = glm::uvec2(interval);
        settings_changed = true;
    }
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    float min_deg = glm::degrees(settings.min_slope_angle);
    float max_deg = glm::degrees(settings.max_slope_angle);
    if (ImGui::DragFloatRange2("Steepness", &min_deg, &max_deg, 0.1f, 0.0f, 90.0f, "Min: %.1f°", "Max: %.1f°", ImGuiSliderFlags_AlwaysClamp)) {
        settings.min_slope_angle = glm::radians(min_deg);
        settings.max_slope_angle = glm::radians(max_deg);
        settings_changed = true;
    }
    rerun |= ImGui::IsItemDeactivatedAfterEdit();

    if (settings_changed)
        m_node->set_settings(settings);
    if (rerun)
        m_node->rerun();
}

} // namespace webgpu_app

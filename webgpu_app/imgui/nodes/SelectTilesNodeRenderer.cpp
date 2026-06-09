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

#include "SelectTilesNodeRenderer.h"

#include <imgui.h>
#include <webgpu_engine/compute/nodes/SelectTilesNode.h>

namespace webgpu_app {
namespace nodes = webgpu_engine::compute::nodes;

SelectTilesNodeRenderer::SelectTilesNodeRenderer(const std::string& name, nodes::SelectTilesNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
{
}

void SelectTilesNodeRenderer::render_settings_content()
{
    static uint32_t zoom_level = m_node->get_settings().zoomlevel;

    const uint32_t min_zoomlevel = 1;
    const uint32_t max_zoomlevel = 18;
    ImGui::SliderScalar("Zoom level", ImGuiDataType_U32, &zoom_level, &min_zoomlevel, &max_zoomlevel);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_node->set_settings(nodes::SelectTilesNode::SelectTilesNodeSettings { zoom_level });
        m_node->rerun();
    }
}

} // namespace webgpu_app

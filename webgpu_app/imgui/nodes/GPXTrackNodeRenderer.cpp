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

#include "GPXTrackNodeRenderer.h"

#include <cstring>
#include <imgui.h>
#include <webgpu_engine/compute/nodes/GPXTrackNode.h>

namespace webgpu_app {
namespace nodes = webgpu_engine::compute::nodes;

GPXTrackNodeRenderer::GPXTrackNodeRenderer(const std::string& name, nodes::GPXTrackNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
{
    const std::string& path = m_node->get_settings().file_path;
    std::strncpy(m_path_buffer.data(), path.c_str(), m_path_buffer.size() - 1);
}

void GPXTrackNodeRenderer::render_settings_content()
{
    auto settings = m_node->get_settings();

    // TODO: replace this plain path field with a file dialog (ImGuiFileDialog on desktop,
    // WebInterop on emscripten/web) once the node frontend code lives in webgpu_app - that
    // avoids injecting webgpu_app's WebInterop into the webgpu_engine layer.
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##gpx_path", m_path_buffer.data(), m_path_buffer.size());
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings.file_path = m_path_buffer.data();
        m_node->set_settings(settings);
        m_node->rerun();
    }

    if (ImGui::Checkbox("Cache (reload only on path change)", &settings.enable_caching)) {
        m_node->set_settings(settings);
        m_node->rerun();
    }
}

} // namespace webgpu_app

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

#include "ExportNodeRenderer.h"

#include <cstring>
#include <imgui.h>
#include <webgpu_engine/compute/nodes/ExportNode.h>

namespace webgpu_app {
namespace nodes = webgpu_engine::compute::nodes;

ExportNodeRenderer::ExportNodeRenderer(const std::string& name, nodes::ExportNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
{
    const auto& s = m_node->get_settings();
    std::strncpy(m_buffer_buf, s.buffer_output_file.c_str(), sizeof(m_buffer_buf) - 1);
    std::strncpy(m_texture_buf, s.texture_output_file.c_str(), sizeof(m_texture_buf) - 1);
    std::strncpy(m_aabb_buf, s.aabb_output_file.c_str(), sizeof(m_aabb_buf) - 1);
}

void ExportNodeRenderer::render_settings_content()
{
    auto settings = m_node->get_settings();
    bool changed = false;

    auto field = [&](const char* label, char* buf, size_t buf_size, std::string& target, const char* socket_name) {
        bool connected = m_node->input_socket(socket_name).is_socket_connected();
        ImGui::TextUnformatted(label);
        if (!connected) {
            ImGui::SameLine();
            ImGui::TextDisabled("(not connected)");
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText((std::string("##") + socket_name).c_str(), buf, buf_size)) {
            target = buf;
            changed = true;
        }
    };

    field("Buffer Output:", m_buffer_buf, sizeof(m_buffer_buf), settings.buffer_output_file, "buffer");
    field("Texture Output:", m_texture_buf, sizeof(m_texture_buf), settings.texture_output_file, "texture");
    field("AABB Output:", m_aabb_buf, sizeof(m_aabb_buf), settings.aabb_output_file, "region aabb");

    if (changed)
        m_node->set_settings(settings);

    ImGui::Spacing();
    ImGui::TextDisabled("Placeholders: {node_name}, {run_datetime}, {run_id}");
}

} // namespace webgpu_app

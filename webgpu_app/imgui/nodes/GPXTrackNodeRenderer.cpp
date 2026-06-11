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
#include <filesystem>
#include <imgui.h>
#include <webgpu_app/ImGuiManager.h>
#include <webgpu_compute/nodes/GPXTrackNode.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

GPXTrackNodeRenderer::GPXTrackNodeRenderer(const std::string& name, nodes::GPXTrackNode& node)
    : NodeRenderer(name, node)
    , m_node(&node)
    , m_dialog_id("gpxnode_" + std::to_string(get_node_id()))
{
    const std::string& path = m_node->get_settings().file_path;
    std::strncpy(m_path_buffer.data(), path.c_str(), m_path_buffer.size() - 1);
}

void GPXTrackNodeRenderer::render_settings_content()
{
    auto settings = m_node->get_settings();

    const float btn_w = ImGui::CalcTextSize("Browse...").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(-btn_w - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##gpx_path", m_path_buffer.data(), m_path_buffer.size());
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings.file_path = m_path_buffer.data();
        m_node->set_settings(settings);
        m_node->rerun();
    }
    ImGui::SameLine();
    m_want_open_dialog = ImGui::Button("Brow#se...");

    if (ImGui::Checkbox("Cache (reload only on path change)", &settings.enable_caching)) {
        m_node->set_settings(settings);
        m_node->rerun();
    }
}

void GPXTrackNodeRenderer::render_dialogs()
{
    m_picked_files.clear();
    if (ImGuiManager::FilePicker(
            m_dialog_id.c_str(), "Choose GPX File", ".gpx,.*", m_want_open_dialog, m_picked_files, /*allow_multiple=*/false, m_last_dialog_directory.c_str())) {
        m_last_dialog_directory = std::filesystem::path(m_picked_files[0]).parent_path().string();
        auto settings = m_node->get_settings();
        settings.file_path = m_picked_files[0];
        std::strncpy(m_path_buffer.data(), settings.file_path.c_str(), m_path_buffer.size() - 1);
        m_node->set_settings(settings);
        m_node->rerun();
    }
}

} // namespace webgpu_app

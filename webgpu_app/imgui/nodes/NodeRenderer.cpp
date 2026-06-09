/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "NodeRenderer.h"

#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <imnodes.h>
#include <iomanip>
#include <qDebug>

namespace webgpu_app {
namespace nodes = webgpu_engine::compute::nodes;

static std::hash<std::string> hasher;

std::string NodeRenderer::format_node_name(const std::string& name)
{
    std::string n = name;
    while (n.find("_node") != std::string::npos)
        n.erase(n.find("_node"), 5);

    for (char& c : n)
        if (c == '_')
            c = ' ';

    bool cap = true;
    for (char& c : n)
        if (std::isspace((unsigned char)c))
            cap = true;
        else if (cap) {
            c = std::toupper((unsigned char)c);
            cap = false;
        }

    return n;
}

ImU32 NodeRenderer::pin_color_for_type(nodes::DataType type)
{
    switch (type) {
    case 0:
        return IM_COL32(255, 160, 50, 255); // tile ID list -> orange
    case 1:
        return IM_COL32(150, 220, 50, 255); // QByteArray list -> yellow-green
    case 2:
        return IM_COL32(70, 130, 255, 255); // TileStorageTexture -> blue
    case 3:
        return IM_COL32(190, 80, 255, 255); // RawBuffer -> purple
    case 4:
        return IM_COL32(50, 210, 210, 255); // TextureWithSampler -> cyan
    case 5:
        return IM_COL32(255, 80, 80, 255); // Aabb -> red
    case 6:
        return IM_COL32(200, 200, 200, 255); // uvec2 -> gray
    default:
        return IM_COL32(255, 255, 255, 255);
    }
}

ImNodesPinShape NodeRenderer::pin_shape_for_type(nodes::DataType type)
{
    switch (type) {
    case 0:
        return ImNodesPinShape_CircleFilled; // tile ID list
    case 1:
        return ImNodesPinShape_CircleFilled; // QByteArray list
    case 2:
        return ImNodesPinShape_QuadFilled; // TileStorageTexture
    case 3:
        return ImNodesPinShape_Quad; // RawBuffer
    case 4:
        return ImNodesPinShape_TriangleFilled; // TextureWithSampler
    case 5:
        return ImNodesPinShape_Triangle; // Aabb
    case 6:
        return ImNodesPinShape_Circle; // uvec2
    default:
        return ImNodesPinShape_CircleFilled;
    }
}

std::string NodeRenderer::format_ms(int duration_in_ms)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    if (duration_in_ms < 1000) {
        ss.str("");
        ss.clear();
        ss << duration_in_ms << " ms";
    } else {
        double seconds = duration_in_ms / 1000.0;
        ss.str("");
        ss.clear();
        ss << seconds << " s";
    }

    return ss.str();
}

NodeRenderer::NodeRenderer(const std::string& name, nodes::Node& node)
    : m_name(name)
    , m_name_formatted(NodeRenderer::format_node_name(m_name))
    , m_node(&node)
    , m_node_id(int(hasher(m_name)))
{
    for (const auto& socket : m_node->input_sockets()) {
        const int socket_id = int(hasher(m_name + socket.name()));
        m_input_socket_ids.push_back(socket_id);
    }

    for (const auto& socket : m_node->output_sockets()) {
        const int socket_id = int(hasher(m_name + socket.name()));
        m_output_socket_ids.push_back(socket_id);
    }
}

ImVec2 NodeRenderer::get_size() const
{
    if (m_size.x >= 0)
        return m_size;
    float width = std::max(100.0f, (float)m_name_formatted.size() * 7.3f);
    size_t num = m_node->input_sockets().size() + m_node->output_sockets().size();
    float height = num * 20.0f;
    return ImVec2(width, height);
}

void NodeRenderer::render(bool reset_position)
{
    if (reset_position) {
        ImNodes::SetNodeEditorSpacePos(m_node_id, m_position);
    }

    bool is_disabled = !m_node->is_enabled();
    bool is_running = m_node->is_running();

    if (is_disabled) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(100, 100, 100, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(100, 100, 100, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(100, 100, 100, 255));
    } else if (is_running) {
        // Dark green title bar while running
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(30, 100, 30, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(40, 120, 40, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(50, 140, 50, 255));
    }

    ImNodes::BeginNode(m_node_id);

    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(m_name_formatted.c_str());
    ImNodes::EndNodeTitleBar();

    render_sockets();

    ImNodes::EndNode();

    // Get size of the node
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    m_size = ImVec2(max.x - min.x, max.y - min.y);

    // Context menu (right-click on node)
    std::string popup_id = "##ctx_" + m_name;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup(popup_id.c_str());

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 8.0f));
    if (ImGui::BeginPopup(popup_id.c_str())) {
        if (ImGui::MenuItem(ICON_FA_REDO "  Rerun"))
            m_node->rerun();
        bool enabled = m_node->is_enabled();
        if (ImGui::MenuItem(enabled ? ICON_FA_TOGGLE_OFF "  Disable" : ICON_FA_TOGGLE_ON "  Enable"))
            m_node->set_enabled(!enabled);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);

    // Get position of the node
    m_position = ImNodes::GetNodeEditorSpacePos(m_node_id);

    // Pop color/style stacks
    if (is_disabled) {
        ImNodes::PopColorStyle(); // selected
        ImNodes::PopColorStyle(); // hovered
        ImNodes::PopColorStyle(); // normal
        ImGui::PopStyleVar();
    } else if (is_running) {
        ImNodes::PopColorStyle(); // selected
        ImNodes::PopColorStyle(); // hovered
        ImNodes::PopColorStyle(); // normal
    }
}

void NodeRenderer::render_sockets()
{
    for (size_t i = 0; i < m_input_socket_ids.size(); i++) {
        const nodes::DataType type = m_node->input_sockets().at(i).type();
        ImNodes::PushColorStyle(ImNodesCol_Pin, pin_color_for_type(type));
        ImNodes::PushColorStyle(ImNodesCol_PinHovered, pin_color_for_type(type));
        ImNodes::BeginInputAttribute(m_input_socket_ids.at(i), pin_shape_for_type(type));
        ImGui::Text("%s", m_node->input_sockets().at(i).name().c_str());
        ImNodes::EndInputAttribute();
        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
    }

    const float node_content_width = get_size().x - 1;
    for (size_t i = 0; i < m_output_socket_ids.size(); i++) {
        const nodes::DataType type = m_node->output_sockets().at(i).type();
        ImNodes::PushColorStyle(ImNodesCol_Pin, pin_color_for_type(type));
        ImNodes::PushColorStyle(ImNodesCol_PinHovered, pin_color_for_type(type));
        ImNodes::BeginOutputAttribute(m_output_socket_ids.at(i), pin_shape_for_type(type));
        const char* label = m_node->output_sockets().at(i).name().c_str();
        const float text_width = ImGui::CalcTextSize(label).x;
        const float text_indent = std::max(0.0f, node_content_width - text_width);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + text_indent);
        ImGui::TextUnformatted(label);
        ImNodes::EndOutputAttribute();
        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
    }
}

int NodeRenderer::get_input_socket_id(const std::string& input_socket_name) const
{
    assert(m_node->has_input_socket(input_socket_name));

    for (size_t i = 0; i < m_node->input_sockets().size(); i++) {
        if (input_socket_name == m_node->input_sockets().at(i).name()) {
            return m_input_socket_ids.at(i);
        }
    }
    qFatal() << "tried to get non-existing input socket " << input_socket_name << " from node renderer for node " << m_name;
    return -1;
}

int NodeRenderer::get_output_socket_id(const std::string& output_socket_name) const
{
    assert(m_node->has_output_socket(output_socket_name));

    for (size_t i = 0; i < m_node->output_sockets().size(); i++) {
        if (output_socket_name == m_node->output_sockets().at(i).name()) {
            return m_output_socket_ids.at(i);
        }
    }
    qFatal() << "tried to get non-existing output socket " << output_socket_name << " from node renderer for node " << m_name;
    return -1;
}

} // namespace webgpu_app

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

#pragma once

#include <QJsonObject>
#include <imgui.h>
#include <imnodes.h>
#include <memory>
#include <string>
#include <vector>

#include <webgpu/compute/nodes/Node.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

class NodeRenderer {
public:
    NodeRenderer(const std::string& name, nodes::Node& node);
    virtual ~NodeRenderer() = default;

    void render(bool reset_position = false);
    void render_sockets();

    virtual bool has_settings() const { return false; }
    virtual void render_settings_content() { }
    // Called outside ImNodes::BeginNodeEditor/EndNodeEditor for proper z-ordering
    virtual void render_dialogs() { }

    // UI-layer serialization: node position (and subclass-specific data).
    virtual QJsonObject serialize_ui() const;
    virtual void deserialize_ui(const QJsonObject& obj);

    int get_node_id() const { return m_node_id; }
    const std::string& get_name() const { return m_name; }
    void rename(const std::string& new_name);

    int get_input_socket_id(const std::string& input_socket_name) const;
    int get_output_socket_id(const std::string& output_socket_name) const;

    void set_position(const ImVec2& position) { m_position = position; }
    ImVec2 get_position() const { return m_position; }
    ImVec2 get_size() const;
    bool is_size_known() const { return m_size.x >= 0; }

    nodes::Node* get_node() const { return m_node; }

    static std::string format_ms(int duration_in_ms);

    static ImU32 pin_color_for_type(nodes::DataType type);
    static ImNodesPinShape pin_shape_for_type(nodes::DataType type);

private:
    std::string m_name;
    nodes::Node* m_node = nullptr;
    int m_node_id = 0;
    std::vector<int> m_input_socket_ids;
    std::vector<int> m_output_socket_ids;

    ImVec2 m_position = { 0, 0 };
    ImVec2 m_size = { -1, -1 }; // Initialized after first frame
};

} // namespace webgpu_app

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

#include <imgui.h>
#include <string>
#include <vector>

#include "nodes/Node.h"

namespace webgpu_engine::compute {

class NodeRenderer {
public:
    NodeRenderer(const std::string& name, nodes::Node& node);
    virtual ~NodeRenderer() = default;

    void render(bool reset_position = false);
    void render_sockets();
    virtual void render_settings();

    int get_input_socket_id(const std::string& input_socket_name) const;
    int get_output_socket_id(const std::string& output_socket_name) const;

    void set_position(const ImVec2& position) { m_position = position; }
    ImVec2 get_position() const { return m_position; }
    ImVec2 get_size() const;

    nodes::Node* get_node() const { return m_node; }

    // Removes optional "_node" and formats the name with capitalization.
    // e.g., "request_height_node" → "Request Height"
    static std::string format_node_name(const std::string& name);

    static std::string format_ms(const int duration_in_ms);

private:
    std::string m_name;
    std::string m_name_formatted;
    nodes::Node* m_node = nullptr;
    int m_node_id = 0;
    std::vector<int> m_input_socket_ids;
    std::vector<int> m_output_socket_ids;

    ImVec2 m_position = { 0, 0 };
    ImVec2 m_size = { -1, -1 }; // Initialized after first frame
};

class SelectTilesNodeRenderer : public NodeRenderer {
public:
    using NodeRenderer::NodeRenderer;
    // void render() override;
};

} // namespace webgpu_engine::compute

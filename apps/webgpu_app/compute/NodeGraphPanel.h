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

#include <QByteArray>
#include <cstdint>
#include <imgui.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "nodes/NodeRenderer.h"
#include "ui/ImGuiPanel.h"
#include <webgpu/compute/NodeGraph.h>

namespace webgpu_engine {
class Context;
}

namespace webgpu_compute::nodes {
class InputSocket;
class OutputSocket;
} // namespace webgpu_compute::nodes

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

class NodeGraphPanel : public ImGuiPanel {
public:
    explicit NodeGraphPanel(webgpu_engine::Context* context);

    // Loads the default pipeline preset into the contexts compute graph (called after init).
    void ready() override;
    void draw() override;

    enum class GraphRenderingMode { Default, Transparent, White };

private:
    // (Re)builds the node renderers for the currently loaded graph.
    void init(nodes::NodeGraph& node_graph);

    // Loads a preset graph from a Qt resource path, wires signals, and inits.
    void load_preset(const std::string& resource_path);

    // Replaces the current graph with a new empty graph.
    void new_graph();

    struct GraphPreset {
        std::string name;
        std::string resource_path;
    };

    void render_error_modal();

    webgpu_engine::Context* m_context = nullptr;
    std::unique_ptr<nodes::NodeGraph> m_owned_graph; // the panel owns the active compute graph
    nodes::NodeGraph* m_node_graph = nullptr;

    std::vector<GraphPreset> m_presets;
    std::optional<std::string> m_pending_preset_path;

    uint32_t m_editor_visible = 0;

    struct ErrorModalState {
        bool should_open = false;
        std::string text;
    };
    ErrorModalState m_error_state;

    ImVec2 m_window_size = ImVec2(0, 0);

    std::unordered_map<const nodes::Node*, ImVec2> m_target_layout;

    bool m_force_node_positions_on_next_frame = false;

    ImVec2 m_initial_node_spacing = ImVec2(50.0f, 50.0f);
    ImVec2 m_canvas_origin = { 0, 0 }; // screen-space top-left of the ImNodes canvas

    std::unordered_map<std::string, std::unique_ptr<NodeRenderer>> m_node_renderers;
    std::unordered_map<const nodes::Node*, NodeRenderer*> m_node_renderers_by_node;
    std::vector<std::pair<int, int>> m_links;

    std::unordered_map<int, nodes::InputSocket*> m_input_socket_by_id;
    std::unordered_map<int, nodes::OutputSocket*> m_output_socket_by_id;

    // Current rendering mode for the graph background and grid.
    GraphRenderingMode m_render_mode = GraphRenderingMode::Default;

    bool m_use_small_font = true;

    bool m_save_dialog_wants_open = false;
    bool m_open_dialog_wants_open = false;
    bool m_pending_auto_layout = false;

    // Add-node popup (Shift+A) and modal (menu)
    bool m_open_add_node_request = false;
    bool m_open_add_node_modal = false;
    ImVec2 m_add_node_popup_pos = { 0, 0 };
    std::vector<std::string> m_registered_node_types; // populated lazily on first open
    int m_add_node_selected_idx = 0;

    // Inline rename state (settings panel)
    char m_rename_buf[128] = {};
    std::string m_rename_current_node; // raw name of node whose name is in m_rename_buf

private:
    // Serializes the current graph (engine + UI positions) as indented JSON bytes.
    QByteArray export_graph_json() const;
    void render_save_dialog();
    void render_open_dialog();
    void render_add_node_popup();

    // Takes ownership of a new graph, wires run/error signals, and calls init().
    void attach_graph(std::unique_ptr<nodes::NodeGraph> graph);

    // Parses JSON bytes, deserializes the graph, applies UI positions (or auto-layouts),
    // and swaps it in. Shows the error modal and keeps the current graph on any failure.
    void import_graph_json(const QByteArray& data, const std::string& source_name);

    void calculate_window_size();
    void center_target_layout();
    void calculate_auto_layout();

    void recenter_graph();
    void reset_graph_layout();

    void push_style();
    void pop_style();

    void render_menu();
    void render_settings_panel();
    void poll_keyboard_shortcuts();

    void rebuild_links();
    void rebuild_socket_id_maps();
    void delete_selected_nodes();
    void rename_selected_node(const std::string& old_name, const std::string& new_name);

    NodeRenderer* find_selected_node_renderer() const;
};

} // namespace webgpu_app

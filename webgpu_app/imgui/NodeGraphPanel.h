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

#include <cstdint>
#include <imgui.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ImGuiPanel.h"
#include "nodes/NodeRenderer.h"
#include <webgpu_engine/compute/NodeGraph.h>

namespace webgpu_engine {
class Context;
}

namespace webgpu_engine::compute::nodes {
class InputSocket;
class OutputSocket;
} // namespace webgpu_engine::compute::nodes

namespace webgpu_app {
namespace nodes = webgpu_engine::compute::nodes;

class NodeGraphPanel : public ImGuiPanel {
public:
    explicit NodeGraphPanel(webgpu_engine::Context* context);

    // Loads the default pipeline preset into the contexts compute graph (called after init).
    void ready() override;
    void draw() override;

    enum class GraphRenderingMode { Default, Transparent, White, WhiteOpaque };

private:
    // (Re)builds the node renderers for the currently loaded graph.
    void init(nodes::NodeGraph& node_graph);

    // Builds the graph for the preset, stores it in the context, wires run/error signals, and inits.
    void load_preset(nodes::NodeGraph::ComputePipelineType type);

    struct PipelinePreset {
        std::string name;
        nodes::NodeGraph::ComputePipelineType type;
    };

    void render_error_modal();

    webgpu_engine::Context* m_context = nullptr;
    nodes::NodeGraph* m_node_graph = nullptr;

    std::vector<PipelinePreset> m_presets;
    nodes::NodeGraph::ComputePipelineType m_active_preset = nodes::NodeGraph::ComputePipelineType::AvalancheTrajectories;
    std::optional<nodes::NodeGraph::ComputePipelineType> m_pending_preset;

    uint32_t m_editor_visible = 0;

    struct ErrorModalState {
        bool should_open = false;
        std::string text;
    };
    ErrorModalState m_error_state;

    ImVec2 m_window_size = ImVec2(0, 0);
    std::string m_window_title;

    std::unordered_map<const nodes::Node*, ImVec2> m_target_layout;
    std::unordered_map<const nodes::Node*, ImVec2> m_start_layout;

    bool m_animation_running = false;
    float m_animation_duration = 0.0f;
    float m_animation_runtime = 0.0f;
    bool m_force_node_positions_on_next_frame = false;
    bool m_first_frame_after_init = false;

    ImVec2 m_initial_node_spacing = ImVec2(50.0f, 50.0f);

    std::unordered_map<std::string, std::unique_ptr<NodeRenderer>> m_node_renderers;
    std::unordered_map<const nodes::Node*, NodeRenderer*> m_node_renderers_by_node;
    std::vector<std::pair<int, int>> m_links;

    std::unordered_map<int, nodes::InputSocket*> m_input_socket_by_id;
    std::unordered_map<int, nodes::OutputSocket*> m_output_socket_by_id;

    // Current rendering mode for the graph background and grid.
    GraphRenderingMode m_render_mode = GraphRenderingMode::Default;

private:
    void calculate_window_size();
    void center_target_layout();
    void calculate_auto_layout();

    void apply_node_layout(float animation_duration);
    void process_animation(float dt);
    void recenter_graph(float animation_duration);
    void reset_graph_layout(float animation_duration);

    void push_style();
    void pop_style();

    void render_menu();
    void render_settings_panel();
    void poll_keyboard_shortcuts();

    void rebuild_links();
    void rebuild_socket_id_maps();

    NodeRenderer* find_selected_node_renderer() const;
};

} // namespace webgpu_app

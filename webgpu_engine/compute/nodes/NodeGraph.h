/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "Node.h"
#include "webgpu_engine/PipelineManager.h"
#include <memory>

namespace webgpu_engine::compute::nodes {

class GraphRunFailureInfo {
public:
    GraphRunFailureInfo() = delete;
    GraphRunFailureInfo(const GraphRunFailureInfo&) = default;

    GraphRunFailureInfo(const std::string& node_name, NodeRunFailureInfo node_run_failure_info);

    [[nodiscard]] const std::string& node_name() const;
    [[nodiscard]] const NodeRunFailureInfo& node_run_failure_info() const;

private:
    std::string m_node_name;
    NodeRunFailureInfo m_node_run_failure_info;
};

// TODO define interface - or maybe for now, just use hardcoded graph for complete normals setup
class NodeGraph : public QObject {
    Q_OBJECT

public:
    NodeGraph(const std::string& name);

    Node* add_node(const std::string& name, std::unique_ptr<Node> node);

    [[nodiscard]] Node& get_node(const std::string& node_name);
    [[nodiscard]] const Node& get_node(const std::string& node_name) const;
    [[nodiscard]] bool exists_node(const std::string& node_name) const;
    [[nodiscard]] const std::string& get_name() const;
    void set_name(const std::string& name);

    [[nodiscard]] std::unordered_map<std::string, std::unique_ptr<Node>>& get_nodes();
    [[nodiscard]] const std::unordered_map<std::string, std::unique_ptr<Node>>& get_nodes() const;

    template <typename NodeType> [[nodiscard]] NodeType& get_node_as(const std::string& node_name) { return static_cast<NodeType&>(get_node(node_name)); }
    template <typename NodeType> [[nodiscard]] const NodeType& get_node_as(const std::string& node_name) const { return static_cast<const NodeType&>(get_node(node_name)); }

    // Enables or disables all nodes whose name contains the given substring.
    void set_enabled_for_nodes_with_name(const std::string& name_substring, bool enabled);

    // obtain outputs - for now all node graphs always output
    //  - a hashmap for overlay tiles (mapping tile id to texture array layer)
    //  - a texture array for overlay tiles
    //  - a hashmap for normals tiles (mapping tile id to texture array layer)
    //  - a texture array for normals tiles
    const GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& output_normals_hash_map() const;
    GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& output_normals_hash_map();
    const TileStorageTexture& output_normals_texture_storage() const;
    TileStorageTexture& output_normals_texture_storage();

    const GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& output_overlay_hash_map() const;
    GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& output_overlay_hash_map();
    const TileStorageTexture& output_overlay_texture_storage() const;
    TileStorageTexture& output_overlay_texture_storage();

private:
    // finds topological order of nodes and connects run_finished and run slots accordingly
    void connect_node_signals_and_slots();

public slots:
    void run();
    void emit_graph_failure(NodeRunFailureInfo info);

signals:
    void run_triggered();
    void run_completed();
    void run_failed(GraphRunFailureInfo info);

public:
    static std::unique_ptr<NodeGraph> create_normal_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_snow_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_release_points_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_avalanche_trajectories_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_trajectories_with_export_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_trajectories_evaluation_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_iterative_simulation_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_fxaa_trajectories_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_avalanche_influence_area_compute_graph(const PipelineManager& manager, WGPUDevice device);
    static std::unique_ptr<NodeGraph> create_d8_compute_graph(const PipelineManager& manager, WGPUDevice device);

private:
    std::string m_name;
    std::unordered_map<std::string, std::unique_ptr<Node>> m_nodes;

    GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>* m_output_normals_hash_map_ptr;
    TileStorageTexture* m_output_normals_texture_storage_ptr;

    GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>* m_output_overlay_hash_map_ptr;
    TileStorageTexture* m_output_overlay_texture_storage_ptr;
};

} // namespace webgpu_engine::compute::nodes

/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2025 Markus Rampp
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

#include "NodeGraph.h"

#include "BufferExportNode.h"
#include "BufferToTextureNode.h"
#include "ComputeAvalancheInfluenceAreaNode.h"
#include "ComputeAvalancheTrajectoriesNode.h"
#include "ComputeD8DirectionsNode.h"
#include "ComputeNormalsNode.h"
#include "ComputeReleasePointsNode.h"
#include "ComputeSnowNode.h"
#include "CreateHashMapNode.h"
#include "DownsampleTilesNode.h"
#include "FxaaNode.h"
#include "IterativeSimulationNode.h"
#include "LoadRegionAabbNode.h"
#include "LoadTextureNode.h"
#include "RequestTilesNode.h"
#include "SelectTilesNode.h"
#include "UpsampleTexturesNode.h"
#include "compute/nodes/HeightDecodeNode.h"
#include "compute/nodes/TileExportNode.h"
#include "compute/nodes/TileStitchNode.h"
#include <QDebug>
#include <memory>

namespace webgpu_engine::compute::nodes {

GraphRunFailureInfo::GraphRunFailureInfo(const std::string& node_name, NodeRunFailureInfo node_run_failure_info)
    : m_node_name(node_name)
    , m_node_run_failure_info(node_run_failure_info)
{
}

const std::string& GraphRunFailureInfo::node_name() const { return m_node_name; }

const NodeRunFailureInfo& GraphRunFailureInfo::node_run_failure_info() const { return m_node_run_failure_info; }

NodeGraph::NodeGraph(const std::string& name)
    : m_name(name)
{
}

Node* NodeGraph::add_node(const std::string& name, std::unique_ptr<Node> node)
{
    assert(!m_nodes.contains(name));
    m_nodes.emplace(name, std::move(node));
    return m_nodes.at(name).get();
}

Node& NodeGraph::get_node(const std::string& node_name) { return *m_nodes.at(node_name); }

const Node& NodeGraph::get_node(const std::string& node_name) const { return *m_nodes.at(node_name); }

bool NodeGraph::exists_node(const std::string& node_name) const { return m_nodes.find(node_name) != m_nodes.end(); }

const std::string& NodeGraph::get_name() const { return m_name; }

void NodeGraph::set_name(const std::string& name) { m_name = name; }

std::unordered_map<std::string, std::unique_ptr<Node>>& NodeGraph::get_nodes() { return m_nodes; }

const std::unordered_map<std::string, std::unique_ptr<Node>>& NodeGraph::get_nodes() const { return m_nodes; }

const GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& NodeGraph::output_normals_hash_map() const { return *m_output_normals_hash_map_ptr; }

GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& NodeGraph::output_normals_hash_map() { return *m_output_normals_hash_map_ptr; }

const TileStorageTexture& NodeGraph::output_normals_texture_storage() const { return *m_output_normals_texture_storage_ptr; }

TileStorageTexture& NodeGraph::output_normals_texture_storage() { return *m_output_normals_texture_storage_ptr; }

const GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& NodeGraph::output_overlay_hash_map() const { return *m_output_overlay_hash_map_ptr; }

GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>& NodeGraph::output_overlay_hash_map() { return *m_output_overlay_hash_map_ptr; }

const TileStorageTexture& NodeGraph::output_overlay_texture_storage() const { return *m_output_overlay_texture_storage_ptr; }

TileStorageTexture& NodeGraph::output_overlay_texture_storage() { return *m_output_overlay_texture_storage_ptr; }

void NodeGraph::connect_node_signals_and_slots()
{
    // basic idea: find topological ordering by counting in-coming edges (in-degree)
    //  1. start with nodes that have no incoming edges
    //  2. select node with 0 incoming edges
    //  3. add it to topological order
    //  4. "remove node" from graph, i.e. update in-degrees of nodes that are connected to outputs of this node
    //   -> this decreases in-degree of other nodes
    //   -> if some nodes reaches zero, add it to queue to for processing next
    //
    // known as https://en.wikipedia.org/wiki/Topological_sorting#Kahn's_algorithm

    assert(!m_nodes.empty());

    std::unordered_map<Node*, uint32_t> in_degrees;
    std::queue<Node*> node_queue;
    std::vector<Node*> topological_ordering;

    for (auto& [_, node] : m_nodes) {
        uint32_t in_degree = 0;
        for (auto& socket : node->input_sockets()) {
            if (socket.is_socket_connected()) {
                in_degree++;
            }
        }
        in_degrees[node.get()] = in_degree;
        if (in_degree == 0) {
            node_queue.push(node.get());
        }
    }

    while (!node_queue.empty()) {
        Node* node = node_queue.front();
        node_queue.pop();
        topological_ordering.push_back(node);
        for (auto& output_socket : node->output_sockets()) {
            for (auto& connected_socket : output_socket.connected_sockets()) {
                auto& connected_node = connected_socket->node();
                in_degrees[&connected_node]--;
                if (in_degrees[&connected_node] == 0) {
                    node_queue.push(&connected_node);
                }
            }
        }
    }

    for (auto& [node, in_degree] : in_degrees) {
        if (in_degree) {
            qFatal() << "cycle in node graph detected";
        }
    }

    connect(this, &NodeGraph::run_triggered, topological_ordering.front(), &Node::run);
    for (uint32_t i = 0; i < topological_ordering.size() - 1; i++) {
        connect(topological_ordering[i], &Node::run_completed, topological_ordering[i + 1], &Node::run);
    }
    connect(topological_ordering.back(), &Node::run_completed, this, &NodeGraph::run_completed); // emits run completed signal in NodeGraph

    for (auto& [_, node] : m_nodes) {
        connect(node.get(), &Node::run_failed, this, &NodeGraph::emit_graph_failure);
    }
}

void NodeGraph::run()
{
    qDebug() << "running node graph ...";
    emit run_triggered();
}

void NodeGraph::emit_graph_failure(NodeRunFailureInfo info)
{
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&info](const auto& key_value_pair) { return key_value_pair.second.get() == &info.node(); });
    assert(it != m_nodes.end());
    emit run_failed(GraphRunFailureInfo(it->first, info));
}

static std::unique_ptr<NodeGraph> create_normal_compute_graph_unconnected(const PipelineManager& manager, WGPUDevice device)
{
    const glm::uvec2 input_resolution = { 65, 65 };

    auto node_graph = std::make_unique<NodeGraph>("normal_compute_graph_unconnected");
    Node* tile_select_node = node_graph->add_node("select_tiles_node", std::make_unique<SelectTilesNode>());
    Node* height_request_node = node_graph->add_node("request_height_node", std::make_unique<RequestTilesNode>());

    ComputeNormalsNode* normal_compute_node
        = static_cast<ComputeNormalsNode*>(node_graph->add_node("compute_normals_node", std::make_unique<ComputeNormalsNode>(manager, device)));

    TileStitchNode::StitchSettings stitch_setting = { .tile_size = input_resolution,
        .tile_has_border = true,
        .texture_format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Uint,
        .texture_usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc };

    TileStitchNode* stitch_node
        = static_cast<TileStitchNode*>(node_graph->add_node("stitch_node", std::make_unique<TileStitchNode>(manager, device, stitch_setting)));

    HeightDecodeNode::HeightDecodeSettings height_decode_settings = {
        .texture_usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc,
    };

    HeightDecodeNode* height_decode_node = static_cast<HeightDecodeNode*>(
        node_graph->add_node("height_decode_node", std::make_unique<HeightDecodeNode>(manager, device, height_decode_settings)));

    // connect height request inputs
    height_request_node->input_socket("tile ids").connect(tile_select_node->output_socket("tile ids"));

    // connect stitch node inputs
    stitch_node->input_socket("tile ids").connect(tile_select_node->output_socket("tile ids"));
    stitch_node->input_socket("texture data").connect(height_request_node->output_socket("tile data"));

    // connect decode node inputs
    height_decode_node->input_socket("region aabb").connect(tile_select_node->output_socket("region aabb"));
    height_decode_node->input_socket("encoded texture").connect(stitch_node->output_socket("texture"));

    // connect normal node inputs
    normal_compute_node->input_socket("bounds").connect(tile_select_node->output_socket("region aabb"));
    normal_compute_node->input_socket("height texture").connect(height_decode_node->output_socket("decoded texture"));

    return node_graph;
}

static std::unique_ptr<NodeGraph> create_release_points_compute_graph_unconnected(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_normal_compute_graph_unconnected(manager, device);

    // add and connect release points node
    Node* release_points_node = node_graph->add_node("compute_release_points_node", std::make_unique<ComputeReleasePointsNode>(manager, device));
    release_points_node->input_socket("normal texture").connect(node_graph->get_node("compute_normals_node").output_socket("normal texture"));

    return node_graph;
}

static std::unique_ptr<NodeGraph> create_trajectories_compute_graph_unconnected(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_release_points_compute_graph_unconnected(manager, device);

    ComputeAvalancheTrajectoriesNode* trajectories_node
        = static_cast<ComputeAvalancheTrajectoriesNode*>(node_graph->add_node("compute_avalanche_trajectories_node", std::make_unique<ComputeAvalancheTrajectoriesNode>(manager, device)));

    BufferToTextureNode::BufferToTextureSettings buffer_to_texture_settings {
        .texture_format = WGPUTextureFormat_RGBA8Unorm,
        .texture_usage = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc),
    };
    BufferToTextureNode* buffer_to_texture_node
        = static_cast<BufferToTextureNode*>(node_graph->add_node("buffer_to_texture_node", std::make_unique<BufferToTextureNode>(manager, device, buffer_to_texture_settings)));

    // connect trajectories node inputs
    trajectories_node->input_socket("region aabb").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));
    trajectories_node->input_socket("normal texture").connect(node_graph->get_node("compute_normals_node").output_socket("normal texture"));
    trajectories_node->input_socket("height texture").connect(node_graph->get_node("height_decode_node").output_socket("decoded texture"));
    trajectories_node->input_socket("release point texture")
        .connect(node_graph->get_node("compute_release_points_node").output_socket("release point texture"));

    // connect buffer to texture node inputs
    buffer_to_texture_node->input_socket("raster dimensions").connect(trajectories_node->output_socket("raster dimensions"));
    // buffer_to_texture_node->input_socket("storage buffer").connect(trajectories_node->output_socket("storage buffer"));
    buffer_to_texture_node->input_socket("storage buffer").connect(trajectories_node->output_socket("layer1_zdelta"));
    buffer_to_texture_node->input_socket("transparency buffer").connect(trajectories_node->output_socket("layer2_cellCounts"));

    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_normal_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_normal_compute_graph_unconnected(manager, device);
    node_graph->set_name("normal_compute_graph");
    node_graph->connect_node_signals_and_slots();
    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_release_points_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_release_points_compute_graph_unconnected(manager, device);
    node_graph->set_name("release_points_compute_graph");
    node_graph->connect_node_signals_and_slots();
    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_snow_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_normal_compute_graph_unconnected(manager, device);
    node_graph->set_name("snow_compute_graph");

    // add and connect snow compute node
    Node* snow_compute_node = node_graph->add_node("compute_snow_node", std::make_unique<ComputeSnowNode>(manager, device));
    snow_compute_node->input_socket("bounds").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));
    snow_compute_node->input_socket("height texture").connect(node_graph->get_node("height_decode_node").output_socket("decoded texture"));
    snow_compute_node->input_socket("normal texture").connect(node_graph->get_node("compute_normals_node").output_socket("normal texture"));

    node_graph->connect_node_signals_and_slots();

    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_avalanche_trajectories_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_trajectories_compute_graph_unconnected(manager, device);
    node_graph->set_name("avalanche_trajectories_compute_graph");
    node_graph->connect_node_signals_and_slots();
    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_trajectories_with_export_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_trajectories_compute_graph_unconnected(manager, device);
    node_graph->set_name("trajectories_with_export_compute_graph");

    TileExportNode::ExportSettings export_settings_rp = { true, true, true, true, "export/release_points" };
    TileExportNode* rp_export_node
        = static_cast<TileExportNode*>(node_graph->add_node("rp_export", std::make_unique<TileExportNode>(device, export_settings_rp)));

    TileExportNode::ExportSettings export_settings_height = { true, true, true, true, "export/heights" };
    TileExportNode* height_export_node
        = static_cast<TileExportNode*>(node_graph->add_node("height_export", std::make_unique<TileExportNode>(device, export_settings_height)));

    TileExportNode::ExportSettings export_settings_trajectories = { true, true, true, true, "export/trajectories" };
    TileExportNode* trajectories_export_node
        = static_cast<TileExportNode*>(node_graph->add_node("trajectories_export", std::make_unique<TileExportNode>(device, export_settings_trajectories)));

    // Connect release points export node
    rp_export_node->input_socket("texture").connect(node_graph->get_node("compute_release_points_node").output_socket("release point texture"));
    rp_export_node->input_socket("region aabb").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));

    // Connect height tiles export node
    height_export_node->input_socket("texture").connect(node_graph->get_node("stitch_node").output_socket("texture"));
    height_export_node->input_socket("region aabb").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));

    // Connect trajectories export node
    trajectories_export_node->input_socket("texture").connect(node_graph->get_node("buffer_to_texture_node").output_socket("texture"));
    trajectories_export_node->input_socket("region aabb").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));

    BufferExportNode* l1_export_node = static_cast<BufferExportNode*>(node_graph->add_node(
        "l1_export_node", std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer1_zdelta.png" })));

    BufferExportNode* l2_export_node = static_cast<BufferExportNode*>(node_graph->add_node("l2_export_node",
        std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer2_cellCounts.png" })));

    BufferExportNode* l3_export_node = static_cast<BufferExportNode*>(node_graph->add_node("l3_export_node",
        std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer3_travelLength.png" })));

    BufferExportNode* l4_export_node = static_cast<BufferExportNode*>(node_graph->add_node("l4_export_node",
        std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer4_travelAngle.png" })));

    BufferExportNode* l5_export_node = static_cast<BufferExportNode*>(node_graph->add_node("l5_export_node",
        std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer5_heightDifference.png" })));

    Node& trajectories_node = node_graph->get_node("compute_avalanche_trajectories_node");
    // connect l1 export node inputs
    l1_export_node->input_socket("buffer").connect(trajectories_node.output_socket("layer1_zdelta"));
    l1_export_node->input_socket("dimensions").connect(trajectories_node.output_socket("raster dimensions"));

    // connect l2 export node inputs
    l2_export_node->input_socket("buffer").connect(trajectories_node.output_socket("layer2_cellCounts"));
    l2_export_node->input_socket("dimensions").connect(trajectories_node.output_socket("raster dimensions"));

    // connect l3 export node inputs
    l3_export_node->input_socket("buffer").connect(trajectories_node.output_socket("layer3_travelLength"));
    l3_export_node->input_socket("dimensions").connect(trajectories_node.output_socket("raster dimensions"));

    // connect l4 export node inputs
    l4_export_node->input_socket("buffer").connect(trajectories_node.output_socket("layer4_travelAngle"));
    l4_export_node->input_socket("dimensions").connect(trajectories_node.output_socket("raster dimensions"));

    // connect l5 export node inputs
    l5_export_node->input_socket("buffer").connect(trajectories_node.output_socket("layer5_altitudeDifference"));
    l5_export_node->input_socket("dimensions").connect(trajectories_node.output_socket("raster dimensions"));

    node_graph->connect_node_signals_and_slots();

    return node_graph;
}

void NodeGraph::set_enabled_for_nodes_with_name(const std::string& name_substring, bool enabled)
{
    int set_count = 0;
    for (auto& [name, node] : m_nodes) {
        if (name.find(name_substring) != std::string::npos) {
            node->set_enabled(enabled);
            set_count++;
        }
    }
    qInfo() << (enabled ? "Enabled" : "Disabled") << set_count << "nodes with name containing:" << QString::fromStdString(name_substring);
}

std::unique_ptr<NodeGraph> NodeGraph::create_trajectories_evaluation_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = std::make_unique<NodeGraph>("trajectories_evaluation_compute_graph");

    Node* load_rp_node = node_graph->add_node("load_rp_node", std::make_unique<LoadTextureNode>(device));
    Node* load_heights_node = node_graph->add_node("load_heights_node", std::make_unique<LoadTextureNode>(device));
    Node* load_aabb_node = node_graph->add_node("load_aabb_node", std::make_unique<LoadRegionAabbNode>());

    ComputeNormalsNode::NormalSettings normals_settings {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .usage = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc),
    };
    ComputeNormalsNode* normal_compute_node = static_cast<ComputeNormalsNode*>(node_graph->add_node("compute_normals_node", std::make_unique<ComputeNormalsNode>(manager, device)));
    normal_compute_node->set_settings(normals_settings);

    HeightDecodeNode::HeightDecodeSettings height_decode_settings = {
        .texture_usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc,
    };

    HeightDecodeNode* height_decode_node = static_cast<HeightDecodeNode*>(node_graph->add_node("height_decode_node", std::make_unique<HeightDecodeNode>(manager, device, height_decode_settings)));

    // connect decode node inputs
    height_decode_node->input_socket("region aabb").connect(load_aabb_node->output_socket("region aabb"));
    height_decode_node->input_socket("encoded texture").connect(load_heights_node->output_socket("texture"));

    // connect normal node inputs
    normal_compute_node->input_socket("bounds").connect(load_aabb_node->output_socket("region aabb"));
    normal_compute_node->input_socket("height texture").connect(height_decode_node->output_socket("decoded texture"));

    // NOTE dont compute release points but load instead - still leaving this code here to get it back up quickly (might be useful for testing angle calculations and stuff)
    // add and connect release points node
    // Node* release_points_node = node_graph->add_node("compute_release_points_node", std::make_unique<ComputeReleasePointsNode>(manager, device));
    // release_points_node->input_socket("normal texture").connect(normal_compute_node->output_socket("normal texture"));

    ComputeAvalancheTrajectoriesNode* trajectories_node
        = static_cast<ComputeAvalancheTrajectoriesNode*>(node_graph->add_node("compute_avalanche_trajectories_node", std::make_unique<ComputeAvalancheTrajectoriesNode>(manager, device)));

    BufferToTextureNode::BufferToTextureSettings buffer_to_texture_settings { .texture_format = WGPUTextureFormat_RGBA8Unorm,
        .texture_usage = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc) };
    BufferToTextureNode* buffer_to_texture_node
        = static_cast<BufferToTextureNode*>(node_graph->add_node("buffer_to_texture_node", std::make_unique<BufferToTextureNode>(manager, device, buffer_to_texture_settings)));

    // connect trajectories node inputs
    trajectories_node->input_socket("region aabb").connect(load_aabb_node->output_socket("region aabb"));
    trajectories_node->input_socket("normal texture").connect(normal_compute_node->output_socket("normal texture"));
    trajectories_node->input_socket("height texture").connect(height_decode_node->output_socket("decoded texture"));
    trajectories_node->input_socket("release point texture").connect(load_rp_node->output_socket("texture"));

    // connect buffer to texture node inputs
    buffer_to_texture_node->input_socket("raster dimensions").connect(trajectories_node->output_socket("raster dimensions"));
    buffer_to_texture_node->input_socket("storage buffer").connect(trajectories_node->output_socket("layer1_zdelta"));
    buffer_to_texture_node->input_socket("transparency buffer").connect(trajectories_node->output_socket("layer2_cellCounts"));

    // === SETUP EXPORT NODES ===
    {
        TileExportNode::ExportSettings export_settings_rp = { true, true, true, true, "export/release_points" };
        TileExportNode* rp_export_node = static_cast<TileExportNode*>(node_graph->add_node("rp_export", std::make_unique<TileExportNode>(device, export_settings_rp)));

        TileExportNode::ExportSettings export_settings_height = { true, true, true, true, "export/heights" };
        TileExportNode* height_export_node = static_cast<TileExportNode*>(node_graph->add_node("height_export", std::make_unique<TileExportNode>(device, export_settings_height)));

        TileExportNode::ExportSettings export_settings_trajectories = { true, true, true, true, "export/trajectories" };
        TileExportNode* trajectories_export_node = static_cast<TileExportNode*>(node_graph->add_node("trajectories_export", std::make_unique<TileExportNode>(device, export_settings_trajectories)));

        TileExportNode::ExportSettings export_settings_normals = { true, true, true, true, "export/normals" };
        TileExportNode* normals_export_node = static_cast<TileExportNode*>(node_graph->add_node("normals_export", std::make_unique<TileExportNode>(device, export_settings_normals)));

        // Connect release points export node
        rp_export_node->input_socket("texture").connect(load_rp_node->output_socket("texture"));
        rp_export_node->input_socket("region aabb").connect(load_aabb_node->output_socket("region aabb"));

        // Connect height tiles export node
        height_export_node->input_socket("texture").connect(load_heights_node->output_socket("texture"));
        height_export_node->input_socket("region aabb").connect(load_aabb_node->output_socket("region aabb"));

        // Connect trajectories export node
        trajectories_export_node->input_socket("texture").connect(buffer_to_texture_node->output_socket("texture"));
        trajectories_export_node->input_socket("region aabb").connect(load_aabb_node->output_socket("region aabb"));

        // Connect normals export node
        normals_export_node->input_socket("texture").connect(normal_compute_node->output_socket("normal texture"));
        normals_export_node->input_socket("region aabb").connect(load_aabb_node->output_socket("region aabb"));

        BufferExportNode* l1_export_node = static_cast<BufferExportNode*>(
            node_graph->add_node("l1_export_node", std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer1_zdelta.png" })));

        BufferExportNode* l2_export_node = static_cast<BufferExportNode*>(
            node_graph->add_node("l2_export_node", std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer2_cellCounts.png" })));

        BufferExportNode* l3_export_node = static_cast<BufferExportNode*>(
            node_graph->add_node("l3_export_node", std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer3_travelLength.png" })));

        BufferExportNode* l4_export_node = static_cast<BufferExportNode*>(
            node_graph->add_node("l4_export_node", std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer4_travelAngle.png" })));

        BufferExportNode* l5_export_node = static_cast<BufferExportNode*>(
            node_graph->add_node("l5_export_node", std::make_unique<BufferExportNode>(device, BufferExportNode::ExportSettings { "export/trajectories/texture_layer5_heightDifference.png" })));

        // connect l1 export node inputs
        l1_export_node->input_socket("buffer").connect(trajectories_node->output_socket("layer1_zdelta"));
        l1_export_node->input_socket("dimensions").connect(trajectories_node->output_socket("raster dimensions"));

        // connect l2 export node inputs
        l2_export_node->input_socket("buffer").connect(trajectories_node->output_socket("layer2_cellCounts"));
        l2_export_node->input_socket("dimensions").connect(trajectories_node->output_socket("raster dimensions"));

        // connect l3 export node inputs
        l3_export_node->input_socket("buffer").connect(trajectories_node->output_socket("layer3_travelLength"));
        l3_export_node->input_socket("dimensions").connect(trajectories_node->output_socket("raster dimensions"));

        // connect l4 export node inputs
        l4_export_node->input_socket("buffer").connect(trajectories_node->output_socket("layer4_travelAngle"));
        l4_export_node->input_socket("dimensions").connect(trajectories_node->output_socket("raster dimensions"));

        // connect l5 export node inputs
        l5_export_node->input_socket("buffer").connect(trajectories_node->output_socket("layer5_altitudeDifference"));
        l5_export_node->input_socket("dimensions").connect(trajectories_node->output_socket("raster dimensions"));
    }

    node_graph->connect_node_signals_and_slots();

    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_iterative_simulation_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_release_points_compute_graph_unconnected(manager, device);
    node_graph->set_name("iterative_simulation_compute_graph");

    IterativeSimulationNode* flowpy_node
        = static_cast<IterativeSimulationNode*>(node_graph->add_node("flowpy", std::make_unique<IterativeSimulationNode>(manager, device)));

    flowpy_node->input_socket("height texture").connect(node_graph->get_node("height_decode_node").output_socket("decoded texture"));
    flowpy_node->input_socket("release point texture").connect(node_graph->get_node("compute_release_points_node").output_socket("release point texture"));

    node_graph->connect_node_signals_and_slots();
    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_fxaa_trajectories_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    auto node_graph = create_trajectories_compute_graph_unconnected(manager, device);
    node_graph->set_name("fxaa_trajectories_compute_graph");
    // fxaa node
    {
        FxaaNode* fxaa_node = static_cast<FxaaNode*>(node_graph->add_node("fxaa_node", std::make_unique<FxaaNode>(manager, device)));

        // Connect release points export node
        fxaa_node->input_socket("texture").connect(node_graph->get_node("buffer_to_texture_node").output_socket("texture"));
    }

    node_graph->connect_node_signals_and_slots();

    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_avalanche_influence_area_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    size_t capacity = 1024;
    glm::uvec2 input_resolution = { 65, 65 };
    glm::uvec2 area_of_influence_output_resolution = { 256, 256 };
    glm::uvec2 upsample_output_resolution = { 256, 256 };

    auto node_graph = std::make_unique<NodeGraph>("avalanche_influence_area_compute_graph");

    Node* target_tile_select_node = node_graph->add_node("select_target_tiles_node", std::make_unique<SelectTilesNode>());
    Node* source_tile_select_node = node_graph->add_node("select_source_tiles_node", std::make_unique<SelectTilesNode>());

    Node* height_request_node = node_graph->add_node("request_height_node", std::make_unique<RequestTilesNode>());
    Node* hash_map_node
        = node_graph->add_node("create_hashmap_node", std::make_unique<CreateHashMapNode>(device, input_resolution, capacity, WGPUTextureFormat_R16Uint));
    ComputeNormalsNode* normal_compute_node
        = static_cast<ComputeNormalsNode*>(node_graph->add_node("compute_normals_node", std::make_unique<ComputeNormalsNode>(manager, device)));
    ComputeAvalancheInfluenceAreaNode* avalanche_influence_area_compute_node
        = static_cast<ComputeAvalancheInfluenceAreaNode*>(node_graph->add_node("compute_area_of_influence_node",
            std::make_unique<ComputeAvalancheInfluenceAreaNode>(manager, device, area_of_influence_output_resolution, capacity, WGPUTextureFormat_RGBA8Unorm)));
    Node* upsample_normals_textures_node
        = node_graph->add_node("upsample_textures_node", std::make_unique<UpsampleTexturesNode>(manager, device, upsample_output_resolution, capacity));
    DownsampleTilesNode* downsample_area_of_influence_tiles_node = static_cast<DownsampleTilesNode*>(
        node_graph->add_node("downsample_area_of_influence_tiles_node", std::make_unique<DownsampleTilesNode>(manager, device, capacity)));
    DownsampleTilesNode* downsample_normals_tiles_node = static_cast<DownsampleTilesNode*>(
        node_graph->add_node("downsample_normals_tiles_node", std::make_unique<DownsampleTilesNode>(manager, device, capacity)));

    // connect tile request node inputs
    source_tile_select_node->output_socket("tile ids").connect(height_request_node->input_socket("tile ids"));

    // connect hash map node inputs
    source_tile_select_node->output_socket("tile ids").connect(hash_map_node->input_socket("tile ids"));
    height_request_node->output_socket("tile data").connect(hash_map_node->input_socket("texture data"));

    // connect normal node inputs
    source_tile_select_node->output_socket("tile ids").connect(normal_compute_node->input_socket("tile ids"));
    hash_map_node->output_socket("hash map").connect(normal_compute_node->input_socket("hash map"));
    hash_map_node->output_socket("textures").connect(normal_compute_node->input_socket("height textures"));

    // connect influence area compute node inputs
    target_tile_select_node->output_socket("tile ids").connect(avalanche_influence_area_compute_node->input_socket("tile ids"));
    normal_compute_node->output_socket("hash map").connect(avalanche_influence_area_compute_node->input_socket("hash map"));
    normal_compute_node->output_socket("normal textures").connect(avalanche_influence_area_compute_node->input_socket("normal textures"));
    hash_map_node->output_socket("textures").connect(avalanche_influence_area_compute_node->input_socket("height textures"));

    // create downsampled area of influence tiles
    target_tile_select_node->output_socket("tile ids").connect(downsample_area_of_influence_tiles_node->input_socket("tile ids"));
    avalanche_influence_area_compute_node->output_socket("hash map").connect(downsample_area_of_influence_tiles_node->input_socket("hash map"));
    avalanche_influence_area_compute_node->output_socket("influence area textures").connect(downsample_area_of_influence_tiles_node->input_socket("textures"));

    // connect upsample textures node inputs
    normal_compute_node->output_socket("normal textures").connect(upsample_normals_textures_node->input_socket("source textures"));

    // connect downsample normal tiles node inputs
    source_tile_select_node->output_socket("tile ids").connect(downsample_normals_tiles_node->input_socket("tile ids"));
    normal_compute_node->output_socket("hash map").connect(downsample_normals_tiles_node->input_socket("hash map"));
    upsample_normals_textures_node->output_socket("output textures").connect(downsample_normals_tiles_node->input_socket("textures"));

    node_graph->m_output_normals_hash_map_ptr = &downsample_normals_tiles_node->hash_map();
    node_graph->m_output_normals_texture_storage_ptr = &downsample_normals_tiles_node->texture_storage();

    node_graph->m_output_overlay_hash_map_ptr = &downsample_area_of_influence_tiles_node->hash_map();
    node_graph->m_output_overlay_texture_storage_ptr = &downsample_area_of_influence_tiles_node->texture_storage();

    node_graph->connect_node_signals_and_slots();

    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_d8_compute_graph(const PipelineManager& manager, WGPUDevice device)
{
    size_t capacity = 1024;
    glm::uvec2 input_resolution = { 65, 65 };
    glm::uvec2 normal_output_resolution = { 65, 65 };

    auto node_graph = std::make_unique<NodeGraph>("d8_compute_graph");
    Node* tile_select_node = node_graph->add_node("select_tiles_node", std::make_unique<SelectTilesNode>());
    Node* height_request_node = node_graph->add_node("request_height_node", std::make_unique<RequestTilesNode>());
    Node* hash_map_node
        = node_graph->add_node("hashmap_node", std::make_unique<CreateHashMapNode>(device, input_resolution, capacity, WGPUTextureFormat_R16Uint));
    ComputeNormalsNode* normal_compute_node
        = static_cast<ComputeNormalsNode*>(node_graph->add_node("compute_normals_node", std::make_unique<ComputeNormalsNode>(manager, device)));
    ComputeD8DirectionsNode* d8_compute_node = static_cast<ComputeD8DirectionsNode*>(
        node_graph->add_node("d8_compute_node", std::make_unique<ComputeD8DirectionsNode>(manager, device, normal_output_resolution, capacity)));

    TileExportNode::ExportSettings export_settings = { true, true, true, true, "height_tiles" };
    TileExportNode* tile_export_node
        = static_cast<TileExportNode*>(node_graph->add_node("tile_export_node", std::make_unique<TileExportNode>(device, export_settings)));

    // connect height request inputs
    tile_select_node->output_socket("tile ids").connect(height_request_node->input_socket("tile ids"));

    // connect height request inputs
    tile_select_node->output_socket("tile ids").connect(hash_map_node->input_socket("tile ids"));
    height_request_node->output_socket("tile data").connect(hash_map_node->input_socket("texture data"));

    // connect normal node inputs
    tile_select_node->output_socket("tile ids").connect(normal_compute_node->input_socket("tile ids"));
    hash_map_node->output_socket("hash map").connect(normal_compute_node->input_socket("hash map"));
    hash_map_node->output_socket("textures").connect(normal_compute_node->input_socket("height textures"));

    // connect d8 node inputs
    tile_select_node->output_socket("tile ids").connect(d8_compute_node->input_socket("tile ids"));
    hash_map_node->output_socket("hash map").connect(d8_compute_node->input_socket("hash map"));
    hash_map_node->output_socket("textures").connect(d8_compute_node->input_socket("height textures"));

    // connect tile export inputs
    tile_select_node->output_socket("tile ids").connect(tile_export_node->input_socket("tile ids"));

    // Export height data
    hash_map_node->output_socket("hash map").connect(tile_export_node->input_socket("hash map"));
    hash_map_node->output_socket("textures").connect(tile_export_node->input_socket("textures"));

    // Export d8 overlay
    // d8_compute_node->output_socket("hash map").connect(tile_export_node->input_socket("hash map"));
    // d8_compute_node->output_socket("d8 direction textures").connect(tile_export_node->input_socket("textures"));

    // node_graph->m_output_normals_hash_map_ptr = &normal_compute_node->hash_map();
    // node_graph->m_output_normals_texture_storage_ptr = &normal_compute_node->texture_storage();
    node_graph->m_output_overlay_hash_map_ptr = &d8_compute_node->hash_map();
    node_graph->m_output_overlay_texture_storage_ptr = &d8_compute_node->texture_storage();

    node_graph->connect_node_signals_and_slots();

    return node_graph;
}

} // namespace webgpu_engine::compute::nodes

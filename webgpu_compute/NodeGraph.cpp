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

#include <QDateTime>

#include "NodeRegistry.h"
#include "nodes/BufferToTextureNode.h"
#include "nodes/ComputeAvalancheTrajectoriesNode.h"
#include "nodes/ComputeNormalsNode.h"
#include "nodes/ComputeReleasePointsNode.h"
#include "nodes/ComputeSnowNode.h"
#include "nodes/ExportNode.h"
#include "nodes/GPXTrackNode.h"
#include "nodes/HeightDecodeNode.h"
#include "nodes/IterativeSimulationNode.h"
#include "nodes/LoadTextureNode.h"
#include "nodes/RequestTilesNode.h"
#include "nodes/SelectTilesNode.h"
#include "nodes/TileStitchNode.h"
#include "nodes/UpsampleTexturesNode.h"
#include <QDebug>
#include <memory>

namespace webgpu_compute::nodes {

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
    node->set_node_name(name);
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

const TileStorageTexture& NodeGraph::output_normals_texture_storage() const { return *m_output_normals_texture_storage_ptr; }

TileStorageTexture& NodeGraph::output_normals_texture_storage() { return *m_output_normals_texture_storage_ptr; }

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

    for (auto& conn : m_topology_connections)
        QObject::disconnect(conn);
    m_topology_connections.clear();

    m_topology_connections.push_back(connect(this, &NodeGraph::run_triggered, topological_ordering.front(), &Node::run));
    for (uint32_t i = 0; i < topological_ordering.size() - 1; i++) {
        m_topology_connections.push_back(connect(topological_ordering[i], &Node::run_completed, topological_ordering[i + 1], &Node::run));
    }
    m_topology_connections.push_back(
        connect(topological_ordering.back(), &Node::run_completed, this, [this](webgpu_compute::GraphRunContext ctx) { emit run_completed(ctx); }));

    for (auto& [_, node] : m_nodes) {
        m_topology_connections.push_back(connect(node.get(), &Node::run_failed, this, &NodeGraph::emit_graph_failure));
    }
}

void NodeGraph::run()
{
    qDebug() << "running node graph ...";

    ++m_run_id;

    std::string run_datetime = QDateTime::currentDateTime().toString("yyyy-MM-ddTHH-mm-ss").toStdString();

    emit run_triggered(webgpu_compute::GraphRunContext { m_run_id, run_datetime });
}

void NodeGraph::emit_graph_failure(NodeRunFailureInfo info)
{
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&info](const auto& key_value_pair) { return key_value_pair.second.get() == &info.node(); });
    assert(it != m_nodes.end());
    emit run_failed(GraphRunFailureInfo(it->first, info));
}

// NOTE: these builders construct nodes directly (and pass per-graph settings) for now. Every built-in
// node is also registered in the NodeRegistry by type-name, so once graphs become data-driven they can
// instead be created via NodeRegistry::create() + configured through the node's settings.
static std::unique_ptr<NodeGraph> create_normal_compute_graph_unconnected(webgpu::Context& ctx)
{
    const glm::uvec2 input_resolution = { 65, 65 };

    auto node_graph = std::make_unique<NodeGraph>("normal_compute_graph_unconnected");
    Node* gpx_track_node = node_graph->add_node("gpx_track_node", std::make_unique<GPXTrackNode>());
    Node* tile_select_node = node_graph->add_node("select_tiles_node", std::make_unique<SelectTilesNode>());
    Node* height_request_node = node_graph->add_node("request_height_node", std::make_unique<RequestTilesNode>());

    ComputeNormalsNode* normal_compute_node = static_cast<ComputeNormalsNode*>(node_graph->add_node("normals_node", std::make_unique<ComputeNormalsNode>(ctx)));

    TileStitchNode::StitchSettings stitch_setting = { .tile_size = input_resolution,
        .tile_has_border = true,
        .texture_format = WGPUTextureFormat::WGPUTextureFormat_RGBA8Uint,
        .texture_usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc };

    TileStitchNode* stitch_node = static_cast<TileStitchNode*>(node_graph->add_node("stitch_node", std::make_unique<TileStitchNode>(ctx, stitch_setting)));

    HeightDecodeNode::HeightDecodeSettings height_decode_settings = {
        .texture_usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc,
    };

    HeightDecodeNode* height_decode_node
        = static_cast<HeightDecodeNode*>(node_graph->add_node("height_decode_node", std::make_unique<HeightDecodeNode>(ctx, height_decode_settings)));

    // connect tile select inputs
    tile_select_node->input_socket("region").connect(gpx_track_node->output_socket("region"));

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

static std::unique_ptr<NodeGraph> create_release_points_compute_graph_unconnected(webgpu::Context& ctx)
{
    auto node_graph = create_normal_compute_graph_unconnected(ctx);

    // add and connect release points node
    Node* release_points_node = node_graph->add_node("release_points_node", std::make_unique<ComputeReleasePointsNode>(ctx));
    release_points_node->input_socket("normal texture").connect(node_graph->get_node("normals_node").output_socket("normal texture"));

    return node_graph;
}

static std::unique_ptr<NodeGraph> create_trajectories_compute_graph_unconnected(webgpu::Context& ctx)
{
    auto node_graph = create_release_points_compute_graph_unconnected(ctx);

    ComputeAvalancheTrajectoriesNode* trajectories_node = static_cast<ComputeAvalancheTrajectoriesNode*>(
        node_graph->add_node("avalanche_trajectories_node", std::make_unique<ComputeAvalancheTrajectoriesNode>(ctx)));

    BufferToTextureNode::BufferToTextureSettings buffer_to_texture_settings {
        .texture_format = WGPUTextureFormat_RGBA8Unorm,
        .texture_usage = (WGPUTextureUsage)(WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc),
    };
    BufferToTextureNode* buffer_to_texture_node = static_cast<BufferToTextureNode*>(
        node_graph->add_node("buffer_to_texture_node", std::make_unique<BufferToTextureNode>(ctx, buffer_to_texture_settings)));

    // connect trajectories node inputs
    trajectories_node->input_socket("region aabb").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));
    trajectories_node->input_socket("normal texture").connect(node_graph->get_node("normals_node").output_socket("normal texture"));
    trajectories_node->input_socket("height texture").connect(node_graph->get_node("height_decode_node").output_socket("decoded texture"));
    trajectories_node->input_socket("release point texture").connect(node_graph->get_node("release_points_node").output_socket("release point texture"));

    // connect buffer to texture node inputs
    buffer_to_texture_node->input_socket("raster dimensions").connect(trajectories_node->output_socket("raster dimensions"));
    buffer_to_texture_node->input_socket("storage buffer").connect(trajectories_node->output_socket("layer1_zdelta"));
    buffer_to_texture_node->input_socket("transparency buffer").connect(trajectories_node->output_socket("layer2_cellCounts"));

    return node_graph;
}

// Adds a terminal OverlayNode that forwards the given result texture (+ the selected
// region aabb) to a consumer. The consuming callback is injected later by the app;
// without it the node is a harmless no-op.
static void add_overlay_node(NodeGraph& node_graph, webgpu::Context& ctx, const std::string& texture_node, const std::string& texture_socket)
{
    // Created by type-name via the registry: the concrete node (OverlayRenderNode) is defined in the
    // renderer layer and registered by the OverlayRenderer, so the compute core needs no dependency on it.
    Node* overlay_node = node_graph.add_node("overlay_node", NodeRegistry::instance().create("OverlayRenderNode", ctx));
    overlay_node->input_socket("texture").connect(node_graph.get_node(texture_node).output_socket(texture_socket));
    overlay_node->input_socket("region aabb").connect(node_graph.get_node("select_tiles_node").output_socket("region aabb"));
}

std::unique_ptr<NodeGraph> NodeGraph::create_preset(ComputePipelineType type, webgpu::Context& ctx)
{
    switch (type) {
    case ComputePipelineType::Snow:
        return create_snow_compute_graph(ctx);
    case ComputePipelineType::AvalancheTrajectories: {
        auto graph = create_trajectories_with_export_compute_graph(ctx);
        graph->set_enabled_for_nodes_with_name("export", false);
        return graph;
    }
    case ComputePipelineType::IterativeSimulation:
        return create_iterative_simulation_compute_graph(ctx);
    }
    return nullptr;
}

std::unique_ptr<NodeGraph> NodeGraph::create_snow_compute_graph(webgpu::Context& ctx)
{
    auto node_graph = create_normal_compute_graph_unconnected(ctx);
    node_graph->set_name("snow_compute_graph");

    // add and connect snow compute node
    Node* snow_compute_node = node_graph->add_node("snow_node", std::make_unique<ComputeSnowNode>(ctx));
    snow_compute_node->input_socket("bounds").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));
    snow_compute_node->input_socket("height texture").connect(node_graph->get_node("height_decode_node").output_socket("decoded texture"));
    snow_compute_node->input_socket("normal texture").connect(node_graph->get_node("normals_node").output_socket("normal texture"));

    add_overlay_node(*node_graph, ctx, "snow_node", "snow texture");
    node_graph->connect_node_signals_and_slots();

    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_avalanche_trajectories_compute_graph(webgpu::Context& ctx)
{
    auto node_graph = create_trajectories_compute_graph_unconnected(ctx);
    node_graph->set_name("avalanche_trajectories_compute_graph");
    add_overlay_node(*node_graph, ctx, "buffer_to_texture_node", "texture");
    node_graph->connect_node_signals_and_slots();
    return node_graph;
}

std::unique_ptr<NodeGraph> NodeGraph::create_trajectories_with_export_compute_graph(webgpu::Context& ctx)
{
    auto node_graph = create_trajectories_compute_graph_unconnected(ctx);
    node_graph->set_name("trajectories_with_export_compute_graph");

    ExportNode* rp_export_node = static_cast<ExportNode*>(node_graph->add_node("rp_export", std::make_unique<ExportNode>(ctx)));

    ExportNode* height_export_node = static_cast<ExportNode*>(node_graph->add_node("height_export", std::make_unique<ExportNode>(ctx)));

    ExportNode* trajectories_export_node = static_cast<ExportNode*>(node_graph->add_node("trajectories_export", std::make_unique<ExportNode>(ctx)));

    // Connect release points export node
    rp_export_node->input_socket("texture").connect(node_graph->get_node("release_points_node").output_socket("release point texture"));
    rp_export_node->input_socket("region aabb").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));

    // Connect height tiles export node
    height_export_node->input_socket("texture").connect(node_graph->get_node("stitch_node").output_socket("texture"));
    height_export_node->input_socket("region aabb").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));

    // Connect trajectories export node
    trajectories_export_node->input_socket("texture").connect(node_graph->get_node("buffer_to_texture_node").output_socket("texture"));
    trajectories_export_node->input_socket("region aabb").connect(node_graph->get_node("select_tiles_node").output_socket("region aabb"));

    ExportNode* l1_export_node = static_cast<ExportNode*>(node_graph->add_node("l1_export_node", std::make_unique<ExportNode>(ctx)));

    ExportNode* l2_export_node = static_cast<ExportNode*>(node_graph->add_node("l2_export_node", std::make_unique<ExportNode>(ctx)));

    ExportNode* l3_export_node = static_cast<ExportNode*>(node_graph->add_node("l3_export_node", std::make_unique<ExportNode>(ctx)));

    ExportNode* l4_export_node = static_cast<ExportNode*>(node_graph->add_node("l4_export_node", std::make_unique<ExportNode>(ctx)));

    ExportNode* l5_export_node = static_cast<ExportNode*>(node_graph->add_node("l5_export_node", std::make_unique<ExportNode>(ctx)));

    Node& trajectories_node = node_graph->get_node("avalanche_trajectories_node");
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

    add_overlay_node(*node_graph, ctx, "buffer_to_texture_node", "texture");
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

std::unique_ptr<NodeGraph> NodeGraph::create_iterative_simulation_compute_graph(webgpu::Context& ctx)
{
    auto node_graph = create_release_points_compute_graph_unconnected(ctx);
    node_graph->set_name("iterative_simulation_compute_graph");

    IterativeSimulationNode* flowpy_node
        = static_cast<IterativeSimulationNode*>(node_graph->add_node("flowpy", std::make_unique<IterativeSimulationNode>(ctx)));

    flowpy_node->input_socket("height texture").connect(node_graph->get_node("height_decode_node").output_socket("decoded texture"));
    flowpy_node->input_socket("release point texture").connect(node_graph->get_node("release_points_node").output_socket("release point texture"));

    add_overlay_node(*node_graph, ctx, "flowpy", "texture");
    node_graph->connect_node_signals_and_slots();
    return node_graph;
}

} // namespace webgpu_compute::nodes

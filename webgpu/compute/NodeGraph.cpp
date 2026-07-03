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
#include <QDebug>
#include <memory>
#include <tl/expected.hpp>

namespace webgpu_compute::nodes {

GraphRunFailureInfo::GraphRunFailureInfo(const std::string& node_name, NodeRunFailureInfo node_run_failure_info)
    : m_node_name(node_name)
    , m_node_run_failure_info(node_run_failure_info)
{
}

const std::string& GraphRunFailureInfo::node_name() const { return m_node_name; }

const NodeRunFailureInfo& GraphRunFailureInfo::node_run_failure_info() const { return m_node_run_failure_info; }

Node* NodeGraph::add_node(const std::string& name, std::unique_ptr<Node> node)
{
    assert(!m_nodes.contains(name));
    node->set_node_name(name);
    m_nodes.emplace(name, std::move(node));
    return m_nodes.at(name).get();
}

void NodeGraph::remove_node(const std::string& name)
{
    auto it = m_nodes.find(name);
    assert(it != m_nodes.end());
    Node* node = it->second.get();

    for (auto& socket : node->input_sockets())
        socket.disconnect();

    for (auto& socket : node->output_sockets()) {
        auto connected = socket.connected_sockets();
        for (auto* input : connected)
            input->disconnect();
    }

    m_nodes.erase(it);
}

void NodeGraph::rename_node(const std::string& old_name, const std::string& new_name)
{
    assert(m_nodes.contains(old_name));
    assert(!m_nodes.contains(new_name));
    auto node = std::move(m_nodes.at(old_name));
    m_nodes.erase(old_name);
    node->set_node_name(new_name);
    m_nodes.emplace(new_name, std::move(node));
}

Node& NodeGraph::get_node(const std::string& node_name) { return *m_nodes.at(node_name); }

const Node& NodeGraph::get_node(const std::string& node_name) const { return *m_nodes.at(node_name); }

bool NodeGraph::exists_node(const std::string& node_name) const { return m_nodes.find(node_name) != m_nodes.end(); }

std::unordered_map<std::string, std::unique_ptr<Node>>& NodeGraph::get_nodes() { return m_nodes; }

const std::unordered_map<std::string, std::unique_ptr<Node>>& NodeGraph::get_nodes() const { return m_nodes; }

tl::expected<std::vector<Node*>, std::string> NodeGraph::compute_topological_order()
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

    if (m_nodes.empty())
        return tl::unexpected(std::string("node graph is empty"));

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
            return tl::unexpected(std::string("cycle in node graph detected"));
        }
    }

    return topological_ordering;
}

void NodeGraph::connect_node_signals_and_slots()
{
    auto order_result = compute_topological_order();
    if (!order_result) {
        qFatal() << "NodeGraph::connect_node_signals_and_slots:" << QString::fromStdString(order_result.error());
    }
    const std::vector<Node*>& topological_ordering = *order_result;

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

} // namespace webgpu_compute::nodes

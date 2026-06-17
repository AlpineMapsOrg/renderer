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

#include "GraphRunContext.h"
#include "nodes/Node.h"
#include <memory>
#include <string>
#include <tl/expected.hpp>
#include <webgpu/base/Context.h>

namespace webgpu_compute::nodes {

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
    NodeGraph() = default;

    Node* add_node(const std::string& name, std::unique_ptr<Node> node);
    void remove_node(const std::string& name);
    void rename_node(const std::string& old_name, const std::string& new_name);

    [[nodiscard]] Node& get_node(const std::string& node_name);
    [[nodiscard]] const Node& get_node(const std::string& node_name) const;
    [[nodiscard]] bool exists_node(const std::string& node_name) const;

    [[nodiscard]] std::unordered_map<std::string, std::unique_ptr<Node>>& get_nodes();
    [[nodiscard]] const std::unordered_map<std::string, std::unique_ptr<Node>>& get_nodes() const;

    template <typename NodeType>
    [[nodiscard]] NodeType& get_node_as(const std::string& node_name)
    {
        return static_cast<NodeType&>(get_node(node_name));
    }
    template <typename NodeType>
    [[nodiscard]] const NodeType& get_node_as(const std::string& node_name) const
    {
        return static_cast<const NodeType&>(get_node(node_name));
    }

    // finds topological order of nodes and connects run_finished and run slots accordingly
    // safe to call multiple times
    [[nodiscard]] tl::expected<std::vector<Node*>, std::string> compute_topological_order();
    void connect_node_signals_and_slots();

public slots:
    void run();
    void emit_graph_failure(NodeRunFailureInfo info);

signals:
    void run_triggered(webgpu_compute::GraphRunContext context);
    void run_completed(webgpu_compute::GraphRunContext context);
    void run_failed(GraphRunFailureInfo info);

private:
    std::unordered_map<std::string, std::unique_ptr<Node>> m_nodes;
    std::vector<QMetaObject::Connection> m_topology_connections;

    uint64_t m_run_id = 0;
};

} // namespace webgpu_compute::nodes

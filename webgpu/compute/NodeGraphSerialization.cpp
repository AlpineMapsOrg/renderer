/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "NodeGraphSerialization.h"

#include "NodeRegistry.h"
#include <QJsonArray>
#include <algorithm>
#include <vector>

namespace webgpu_compute::nodes {

QJsonObject serialize_node_graph(const NodeGraph& graph)
{
    QJsonObject root;
    root["format"] = QLatin1String(NODE_GRAPH_JSON_FORMAT);
    root["version"] = NODE_GRAPH_JSON_VERSION;

    // m_nodes is an unordered_map; sort by name for stable, diffable output
    std::vector<std::pair<std::string, const Node*>> sorted_nodes;
    sorted_nodes.reserve(graph.get_nodes().size());
    for (const auto& [name, node] : graph.get_nodes())
        sorted_nodes.emplace_back(name, node.get());
    std::sort(sorted_nodes.begin(), sorted_nodes.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    QJsonArray nodes_array;
    QJsonArray connections_array;
    for (const auto& [name, node] : sorted_nodes) {
        QJsonObject node_object;
        node_object["name"] = QString::fromStdString(name);
        node_object["type"] = QString::fromStdString(node->get_type_name());
        node_object["enabled"] = node->is_enabled();
        QJsonObject settings;
        node->serialize_settings(settings);
        if (!settings.isEmpty())
            node_object["settings"] = settings;
        nodes_array.append(node_object);

        // Collecting connections from the input side (each input has at most one source)
        // yields each connection exactly once, in deterministic order.
        for (const InputSocket& input_socket : node->input_sockets()) {
            if (!input_socket.is_socket_connected())
                continue;
            const OutputSocket& output_socket = input_socket.connected_socket();
            QJsonObject from;
            from["node"] = QString::fromStdString(output_socket.node().get_node_name());
            from["socket"] = QString::fromStdString(output_socket.name());
            QJsonObject to;
            to["node"] = QString::fromStdString(name);
            to["socket"] = QString::fromStdString(input_socket.name());
            QJsonObject connection;
            connection["from"] = from;
            connection["to"] = to;
            connections_array.append(connection);
        }
    }
    root["nodes"] = nodes_array;
    root["connections"] = connections_array;
    return root;
}

tl::expected<std::unique_ptr<NodeGraph>, std::string> deserialize_node_graph(const QJsonObject& root, webgpu::Context& ctx)
{
    // Format / version guard
    if (root["format"].toString() != QLatin1String(NODE_GRAPH_JSON_FORMAT))
        return tl::unexpected(std::string("invalid format tag (expected \"") + NODE_GRAPH_JSON_FORMAT + "\")");
    if (root["version"].toInt(-1) != NODE_GRAPH_JSON_VERSION)
        return tl::unexpected(std::string("unsupported version (expected ") + std::to_string(NODE_GRAPH_JSON_VERSION) + ")");

    auto graph = std::make_unique<NodeGraph>();

    // Create nodes
    const QJsonArray nodes_array = root["nodes"].toArray();
    for (const QJsonValue& val : nodes_array) {
        const QJsonObject node_obj = val.toObject();
        const std::string node_name = node_obj["name"].toString().toStdString();
        const std::string type_name = node_obj["type"].toString().toStdString();

        if (node_name.empty())
            return tl::unexpected(std::string("node entry is missing \"name\""));
        if (graph->exists_node(node_name))
            return tl::unexpected("duplicate node name: \"" + node_name + "\"");

        auto node = NodeRegistry::instance().try_create(type_name, ctx);
        if (!node)
            return tl::unexpected("unknown node type: \"" + type_name + "\"");

        node->set_enabled(node_obj["enabled"].toBool(true));
        if (node_obj.contains("settings"))
            node->deserialize_settings(node_obj["settings"].toObject());

        graph->add_node(node_name, std::move(node));
    }

    // Wire connections
    const QJsonArray connections_array = root["connections"].toArray();
    for (const QJsonValue& val : connections_array) {
        const QJsonObject conn = val.toObject();
        const QJsonObject from_obj = conn["from"].toObject();
        const QJsonObject to_obj = conn["to"].toObject();

        const std::string from_node = from_obj["node"].toString().toStdString();
        const std::string from_socket = from_obj["socket"].toString().toStdString();
        const std::string to_node = to_obj["node"].toString().toStdString();
        const std::string to_socket = to_obj["socket"].toString().toStdString();

        if (!graph->exists_node(from_node))
            return tl::unexpected("connection references unknown source node \"" + from_node + "\"");
        if (!graph->exists_node(to_node))
            return tl::unexpected("connection references unknown destination node \"" + to_node + "\"");

        Node& src = graph->get_node(from_node);
        Node& dst = graph->get_node(to_node);

        if (!src.has_output_socket(from_socket))
            return tl::unexpected("node \"" + from_node + "\" has no output socket \"" + from_socket + "\"");
        if (!dst.has_input_socket(to_socket))
            return tl::unexpected("node \"" + to_node + "\" has no input socket \"" + to_socket + "\"");

        OutputSocket& output = src.output_socket(from_socket);
        InputSocket& input = dst.input_socket(to_socket);

        if (output.type() != input.type())
            return tl::unexpected("type mismatch: \"" + from_node + "\":\"" + from_socket + "\" -> \"" + to_node + "\":\"" + to_socket + "\"");

        input.connect(output);
    }

    // Cycle / empty check before wiring Qt signals
    auto topo = graph->compute_topological_order();
    if (!topo)
        return tl::unexpected(topo.error());

    graph->connect_node_signals_and_slots();
    return graph;
}

} // namespace webgpu_compute::nodes

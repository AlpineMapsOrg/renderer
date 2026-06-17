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

#pragma once

#include "NodeGraph.h"
#include <QJsonObject>
#include <memory>
#include <string>
#include <tl/expected.hpp>
#include <webgpu/base/Context.h>

namespace webgpu_compute::nodes {

// JSON schema (version 1):
// {
//   "format": "webigeo/node-graph",
//   "version": 1,
//   "name": "<graph name>",
//   "nodes": [ { "name": ..., "type": ..., "enabled": ..., "settings": {...} }, ... ],
//   "connections": [ { "from": { "node": ..., "socket": ... }, "to": { "node": ..., "socket": ... } }, ... ],
//   "ui": { "nodes": { "<node name>": {...} } }   // optional, written/consumed by the app layer only
// }
//
// Unknown keys are ignored; missing settings keys keep their C++ defaults.

inline constexpr const char* NODE_GRAPH_JSON_FORMAT = "webigeo/node-graph";
inline constexpr int NODE_GRAPH_JSON_VERSION = 1;

/// Serializes the engine state of the graph (no "ui" section). Nodes are sorted by
/// name so repeated saves of the same graph are byte-identical.
[[nodiscard]] QJsonObject serialize_node_graph(const NodeGraph& graph);

/// Creates a graph from JSON, with full validation (format/version, duplicate or unknown
/// node types, unknown sockets, socket type mismatches, cycles). On success the returned
/// graph is fully wired (connect_node_signals_and_slots has been called). Ignores "ui".
[[nodiscard]] tl::expected<std::unique_ptr<NodeGraph>, std::string> deserialize_node_graph(const QJsonObject& root, webgpu::Context& ctx);

} // namespace webgpu_compute::nodes

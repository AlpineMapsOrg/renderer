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
#include <QDebug>
#include <cassert>

namespace webgpu_compute {

NodeRegistry::NodeRegistry()
{
    // ToDo: Make sure all nodes share the same constructor and create Makro for register process
    register_node("GPXTrackNode", [](webgpu::Context&) { return std::make_unique<nodes::GPXTrackNode>(); });
    register_node("SelectTilesNode", [](webgpu::Context&) { return std::make_unique<nodes::SelectTilesNode>(); });
    register_node("RequestTilesNode", [](webgpu::Context&) { return std::make_unique<nodes::RequestTilesNode>(); });
    register_node("ComputeNormalsNode", [](webgpu::Context& c) { return std::make_unique<nodes::ComputeNormalsNode>(c); });
    register_node("TileStitchNode", [](webgpu::Context& c) { return std::make_unique<nodes::TileStitchNode>(c); });
    register_node("HeightDecodeNode", [](webgpu::Context& c) { return std::make_unique<nodes::HeightDecodeNode>(c); });
    register_node("ComputeReleasePointsNode", [](webgpu::Context& c) { return std::make_unique<nodes::ComputeReleasePointsNode>(c); });
    register_node("ComputeAvalancheTrajectoriesNode", [](webgpu::Context& c) { return std::make_unique<nodes::ComputeAvalancheTrajectoriesNode>(c); });
    register_node("BufferToTextureNode", [](webgpu::Context& c) { return std::make_unique<nodes::BufferToTextureNode>(c); });
    register_node("ComputeSnowNode", [](webgpu::Context& c) { return std::make_unique<nodes::ComputeSnowNode>(c); });
    register_node("IterativeSimulationNode", [](webgpu::Context& c) { return std::make_unique<nodes::IterativeSimulationNode>(c); });
    register_node("ExportNode", [](webgpu::Context& c) { return std::make_unique<nodes::ExportNode>(c); });
    register_node("LoadTextureNode", [](webgpu::Context& c) { return std::make_unique<nodes::LoadTextureNode>(c); });
}

NodeRegistry& NodeRegistry::instance()
{
    static NodeRegistry registry;
    return registry;
}

void NodeRegistry::register_node(const std::string& type_name, NodeFactory factory) { m_factories[type_name] = std::move(factory); }

bool NodeRegistry::is_registered(const std::string& type_name) const { return m_factories.find(type_name) != m_factories.end(); }

std::unique_ptr<nodes::Node> NodeRegistry::create(const std::string& type_name, webgpu::Context& ctx) const
{
    const auto it = m_factories.find(type_name);
    if (it == m_factories.end()) {
        qCritical() << "NodeRegistry::create: no node registered for type" << QString::fromStdString(type_name);
        assert(false && "NodeRegistry::create: unknown node type");
        return nullptr;
    }
    return it->second(ctx);
}

} // namespace webgpu_compute

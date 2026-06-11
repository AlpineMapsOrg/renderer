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

#include "NodeRendererFactory.h"

#include "BufferToTextureNodeRenderer.h"
#include "ComputeAvalancheTrajectoriesNodeRenderer.h"
#include "ComputeReleasePointsNodeRenderer.h"
#include "ComputeSnowNodeRenderer.h"
#include "ExportNodeRenderer.h"
#include "GPXTrackNodeRenderer.h"
#include "NodeRenderer.h"
#include "OverlayNodeRenderer.h"
#include "RequestTilesNodeRenderer.h"
#include "SelectTilesNodeRenderer.h"
#include <webgpu_compute/nodes/BufferToTextureNode.h>
#include <webgpu_compute/nodes/ComputeAvalancheTrajectoriesNode.h>
#include <webgpu_compute/nodes/ComputeReleasePointsNode.h>
#include <webgpu_compute/nodes/ComputeSnowNode.h>
#include <webgpu_compute/nodes/ExportNode.h>
#include <webgpu_compute/nodes/GPXTrackNode.h>
#include <webgpu_compute/nodes/RequestTilesNode.h>
#include <webgpu_compute/nodes/SelectTilesNode.h>

#include "compute/OverlayRenderNode.h"

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

std::unique_ptr<NodeRenderer> NodeRendererFactory::create(const std::string& name, nodes::Node& node)
{
    if (auto* n = dynamic_cast<nodes::ExportNode*>(&node))
        return std::make_unique<ExportNodeRenderer>(name, *n);
    if (auto* n = dynamic_cast<nodes::OverlayRenderNode*>(&node))
        return std::make_unique<OverlayNodeRenderer>(name, *n);
    if (auto* n = dynamic_cast<nodes::BufferToTextureNode*>(&node))
        return std::make_unique<BufferToTextureNodeRenderer>(name, *n);
    if (auto* n = dynamic_cast<nodes::ComputeAvalancheTrajectoriesNode*>(&node))
        return std::make_unique<ComputeAvalancheTrajectoriesNodeRenderer>(name, *n);
    if (auto* n = dynamic_cast<nodes::ComputeReleasePointsNode*>(&node))
        return std::make_unique<ComputeReleasePointsNodeRenderer>(name, *n);
    if (auto* n = dynamic_cast<nodes::ComputeSnowNode*>(&node))
        return std::make_unique<ComputeSnowNodeRenderer>(name, *n);
    if (auto* n = dynamic_cast<nodes::RequestTilesNode*>(&node))
        return std::make_unique<RequestTilesNodeRenderer>(name, *n);
    if (auto* n = dynamic_cast<nodes::SelectTilesNode*>(&node))
        return std::make_unique<SelectTilesNodeRenderer>(name, *n);
    if (auto* n = dynamic_cast<nodes::GPXTrackNode*>(&node))
        return std::make_unique<GPXTrackNodeRenderer>(name, *n);
    return std::make_unique<NodeRenderer>(name, node);
}

} // namespace webgpu_app

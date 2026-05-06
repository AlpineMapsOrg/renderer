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
#include "NodeRenderer.h"
#include "RequestTilesNodeRenderer.h"
#include "../../nodes/BufferToTextureNode.h"
#include "../../nodes/ComputeAvalancheTrajectoriesNode.h"
#include "../../nodes/ComputeReleasePointsNode.h"
#include "../../nodes/ComputeSnowNode.h"
#include "../../nodes/ExportNode.h"
#include "../../nodes/RequestTilesNode.h"

namespace webgpu_engine::compute {

std::unique_ptr<NodeRenderer> NodeRendererFactory::create(const std::string& name, nodes::Node& node)
{
    if (auto* n = dynamic_cast<nodes::ExportNode*>(&node))
        return std::make_unique<ExportNodeRenderer>(name, *n);
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
    return std::make_unique<NodeRenderer>(name, node);
}

} // namespace webgpu_engine::compute

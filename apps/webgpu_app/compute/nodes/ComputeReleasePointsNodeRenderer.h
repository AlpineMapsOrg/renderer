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

#include "NodeRenderer.h"

namespace webgpu_compute::nodes {
class ComputeReleasePointsNode;
}

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

class ComputeReleasePointsNodeRenderer : public NodeRenderer {
public:
    ComputeReleasePointsNodeRenderer(const std::string& name, nodes::ComputeReleasePointsNode& node);
    bool has_settings() const override { return true; }
    void render_settings_content() override;

private:
    nodes::ComputeReleasePointsNode* m_node;
};

} // namespace webgpu_app

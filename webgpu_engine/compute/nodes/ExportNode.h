/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2024 Patrick Komon
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

#include "Node.h"
#include <webgpu/Context.h>

namespace webgpu_engine::compute::nodes {

// General-purpose export node. Connect any combination of:
//   - "texture" -> exports a GPU texture to an image file
//   - "buffer" + "dimensions" -> exports a uint32 GPU buffer to an image file
//   - "region aabb" -> writes a bounding-box text file
class ExportNode : public Node {
    Q_OBJECT

public:
    NODE_TYPE_NAME(ExportNode)

    struct ExportSettings {
        // Supported placeholders: {node_name}, {run_datetime}, {run_id}
        std::string buffer_output_file = "export/{run_datetime}_{run_id}/exp_{node_name}_buff.png";
        std::string texture_output_file = "export/{run_datetime}_{run_id}/exp_{node_name}_tex.png";
        std::string aabb_output_file = "export/{run_datetime}_{run_id}/exp_aabb.txt";
    };

    explicit ExportNode(webgpu::Context& ctx);
    ExportNode(webgpu::Context& ctx, const ExportSettings& settings);

    const ExportSettings& get_settings() const { return m_settings; }
    void set_settings(const ExportSettings& settings);

public slots:
    void run_impl() override;

private:
    webgpu::Context* m_ctx;
    ExportSettings m_settings;
};

} // namespace webgpu_engine::compute::nodes

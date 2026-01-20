/*****************************************************************************
 * weBIGeo
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

#include "Node.h"

namespace webgpu_engine::compute::nodes {

class BufferExportNode : public Node {
    Q_OBJECT

public:
    struct ExportSettings {
        std::string output_file = "output_buffer.png";
    };

    BufferExportNode(WGPUDevice device, const ExportSettings& settings);

    void set_settings(const ExportSettings& settings);

public slots:
    void run_impl() override;

private:
    WGPUDevice m_device;
    ExportSettings m_settings;
};

} // namespace webgpu_engine::compute::nodes

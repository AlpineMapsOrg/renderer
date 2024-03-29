/*****************************************************************************
 * weBIGeo
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

#include <webgpu/webgpu.h>

namespace webgpu_engine::raii {

/// Represents a (web)GPU render pipeline object.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
class RenderPipeline {
public:
    RenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor& desc);
    ~RenderPipeline();

    // delete copy constructor and copy-assignment operator
    RenderPipeline(const RenderPipeline& other) = delete;
    RenderPipeline& operator=(const RenderPipeline& other) = delete;

    WGPURenderPipeline handle() const;

private:
    WGPURenderPipeline m_render_pipeline;
};

} // namespace webgpu_engine::raii

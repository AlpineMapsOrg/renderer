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

#include "BindGroupLayout.h"
#include "base_types.h"
#include "webgpu_engine/Framebuffer.h"
#include "webgpu_engine/util/VertexBufferInfo.h"
#include <memory>
#include <vector>

namespace webgpu_engine::raii {

class GenericRenderPipeline {
public:
    using VertexBufferInfos = std::vector<webgpu_engine::util::SingleVertexBufferInfo>;
    using BindGroupLayouts = std::vector<const webgpu_engine::raii::BindGroupLayout*>;

    GenericRenderPipeline(WGPUDevice device, const ShaderModule& vertex_shader, const ShaderModule& fragment_shader,
        const VertexBufferInfos& vertex_buffer_infos, const FramebufferFormat& framebuffer_format, const BindGroupLayouts& bind_group_layouts,
        const std::vector<std::optional<WGPUBlendState>>& blend_states = {});

    const RenderPipeline& pipeline() const;
    const FramebufferFormat& framebuffer_format() const;

private:
    std::unique_ptr<RenderPipeline> m_pipeline;
    std::unique_ptr<PipelineLayout> m_pipeline_layout;

    FramebufferFormat m_framebuffer_format;
};

} // namespace webgpu_engine::raii

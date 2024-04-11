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

#include "base_types.h"
#include "webgpu_engine/Framebuffer.h"
#include "webgpu_engine/util/VertexBufferInfo.h"
#include <cassert>
#include <memory>
#include <vector>

namespace webgpu_engine::raii {

enum class BlendPreset { DEFAULT };

constexpr WGPUBlendState get_blend_preset(BlendPreset preset)
{
    switch (preset) {
    case BlendPreset::DEFAULT:
        return { .color = { .operation = WGPUBlendOperation::WGPUBlendOperation_Add,
                     .srcFactor = WGPUBlendFactor::WGPUBlendFactor_SrcAlpha,
                     .dstFactor = WGPUBlendFactor::WGPUBlendFactor_OneMinusSrcAlpha },
            .alpha = { .operation = WGPUBlendOperation::WGPUBlendOperation_Add,
                .srcFactor = WGPUBlendFactor::WGPUBlendFactor_Zero,
                .dstFactor = WGPUBlendFactor::WGPUBlendFactor_One } };
    }
    assert(false);
    return {};
}

struct ColorTargetInfo {
    BlendPreset blend_preset {};
    WGPUTextureFormat texture_format {};
};

class GenericRenderPipeline {
public:
    using VertexBufferInfos = std::vector<webgpu_engine::util::SingleVertexBufferInfo>;
    using ColorTargetInfos = std::vector<webgpu_engine::raii::ColorTargetInfo>;
    using BindGroupLayouts = std::vector<const webgpu_engine::raii::BindGroupLayout*>;

    GenericRenderPipeline(WGPUDevice device, const ShaderModule& vertex_shader, const ShaderModule& fragment_shader,
        const VertexBufferInfos& vertex_buffer_infos, const FramebufferFormat& framebuffer_format, const BindGroupLayouts& bind_group_layouts,
        WGPUBlendState blend_state = get_blend_preset(BlendPreset::DEFAULT));

    const RenderPipeline& pipeline() const;

private:
    std::unique_ptr<RenderPipeline> m_pipeline;
    std::unique_ptr<PipelineLayout> m_pipeline_layout;
};

} // namespace webgpu_engine::raii

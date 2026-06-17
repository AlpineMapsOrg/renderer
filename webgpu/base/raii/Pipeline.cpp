/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "Pipeline.h"

#include <algorithm>
#include <iterator>
#include <webgpu/webgpu.h>

namespace webgpu::raii {

GenericRenderPipeline::GenericRenderPipeline(WGPUDevice device, const ShaderModule& vertex_shader, const ShaderModule& fragment_shader,
    const VertexBufferInfos& vertex_buffer_infos, const FramebufferFormat& framebuffer_format, const BindGroupLayouts& bind_group_layouts,
    const std::vector<std::optional<WGPUBlendState>>& blend_states)
    : m_framebuffer_format { framebuffer_format }
{
    assert(blend_states.size() <= framebuffer_format.color_formats.size());

    std::vector<WGPUColorTargetState> color_target_states;

    for (size_t i = 0; i < framebuffer_format.color_formats.size(); i++) {
        WGPUColorTargetState color_target_state {};
        color_target_state.format = framebuffer_format.color_formats.at(i);
        if (i < blend_states.size() && blend_states.at(i).has_value()) {
            color_target_state.blend = &blend_states.at(i).value();
        }
        color_target_state.writeMask = WGPUColorWriteMask_All;
        color_target_states.push_back(color_target_state);
    }

    WGPUFragmentState fragment_state {};
    fragment_state.module = fragment_shader.handle();
    fragment_state.entryPoint = WGPUStringView { .data = "fragmentMain", .length = WGPU_STRLEN };
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;
    fragment_state.targetCount = color_target_states.size();
    fragment_state.targets = color_target_states.data();

    std::vector<WGPUBindGroupLayout> bind_group_layout_handles;
    std::transform(bind_group_layouts.begin(), bind_group_layouts.end(), std::back_insert_iterator(bind_group_layout_handles),
        [](const BindGroupLayout* layout) { return layout->handle(); });
    m_pipeline_layout = std::make_unique<PipelineLayout>(device, bind_group_layout_handles);

    std::vector<WGPUVertexBufferLayout> layouts;
    std::transform(vertex_buffer_infos.begin(), vertex_buffer_infos.end(), std::back_insert_iterator(layouts),
        [](const util::SingleVertexBufferInfo& info) { return info.vertex_buffer_layout(); });

    WGPURenderPipelineDescriptor pipeline_desc {};
    pipeline_desc.vertex.module = vertex_shader.handle();
    pipeline_desc.vertex.entryPoint = WGPUStringView { .data = "vertexMain", .length = WGPU_STRLEN };
    pipeline_desc.vertex.bufferCount = layouts.size();
    pipeline_desc.vertex.buffers = layouts.data();
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants = nullptr;
    pipeline_desc.primitive.topology = WGPUPrimitiveTopology::WGPUPrimitiveTopology_TriangleStrip;
    pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat::WGPUIndexFormat_Uint16;
    pipeline_desc.primitive.frontFace = WGPUFrontFace::WGPUFrontFace_CCW;
    pipeline_desc.primitive.cullMode = WGPUCullMode::WGPUCullMode_None;
    pipeline_desc.fragment = &fragment_state;

    WGPUStencilFaceState stencil_face_state {};
    WGPUDepthStencilState depth_stencil_state {};
    if (framebuffer_format.depth_format != WGPUTextureFormat_Undefined) {
        // needed to disable stencil test (for dawn), see https://github.com/ocornut/imgui/issues/7232
        stencil_face_state.compare = WGPUCompareFunction::WGPUCompareFunction_Always;
        stencil_face_state.depthFailOp = WGPUStencilOperation::WGPUStencilOperation_Keep;
        stencil_face_state.failOp = WGPUStencilOperation::WGPUStencilOperation_Keep;
        stencil_face_state.passOp = WGPUStencilOperation::WGPUStencilOperation_Keep;

        depth_stencil_state.depthCompare = WGPUCompareFunction::WGPUCompareFunction_GreaterEqual;
        depth_stencil_state.depthWriteEnabled
            = (framebuffer_format.depth_format != WGPUTextureFormat_Undefined) ? WGPUOptionalBool_True : WGPUOptionalBool_False;
        depth_stencil_state.stencilReadMask = 0;
        depth_stencil_state.stencilWriteMask = 0;
        depth_stencil_state.stencilFront = stencil_face_state;
        depth_stencil_state.stencilBack = stencil_face_state;
        depth_stencil_state.format = framebuffer_format.depth_format;
        pipeline_desc.depthStencil = &depth_stencil_state;
    } else {
        pipeline_desc.depthStencil = nullptr;
    }

    pipeline_desc.multisample.count = 1;
    pipeline_desc.multisample.mask = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;
    pipeline_desc.layout = m_pipeline_layout->handle();

    m_pipeline = std::make_unique<raii::RenderPipeline>(device, pipeline_desc);
}

const RenderPipeline& GenericRenderPipeline::pipeline() const { return *m_pipeline; }

const FramebufferFormat& GenericRenderPipeline::framebuffer_format() const { return m_framebuffer_format; }

} // namespace webgpu::raii

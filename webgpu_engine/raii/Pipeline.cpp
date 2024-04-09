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

#include "Pipeline.h"

#include <algorithm>
#include <iterator>
#include <webgpu/webgpu.h>

namespace webgpu_engine::raii {

GenericRenderPipeline::GenericRenderPipeline(WGPUDevice device, const ShaderModule& vertex_shader, const ShaderModule& fragment_shader,
    const VertexBufferInfos& vertex_buffer_infos, const ColorTargetInfos& color_target_infos, const BindGroupLayouts& bind_group_layouts,
    const WGPUTextureFormat depth_texture_format)
{
    std::vector<WGPUBlendState> blend_states;
    std::vector<WGPUColorTargetState> color_target_states;
    std::for_each(color_target_infos.begin(), color_target_infos.end(), [&blend_states, &color_target_states](const ColorTargetInfo& info) {
        blend_states.push_back(get_blend_preset(info.blend_preset));
        WGPUColorTargetState color_target_state {};
        color_target_state.blend = &blend_states.back();
        color_target_state.format = info.texture_format;
        color_target_state.writeMask = WGPUColorWriteMask::WGPUColorWriteMask_All;
        color_target_states.push_back(color_target_state);
    });

    WGPUFragmentState fragment_state {};
    fragment_state.module = fragment_shader.handle();
    fragment_state.entryPoint = "fragmentMain";
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;
    fragment_state.targetCount = color_target_states.size();
    fragment_state.targets = color_target_states.data();

    // needed to disable stencil test (for dawn), see https://github.com/ocornut/imgui/issues/7232
    WGPUStencilFaceState stencil_face_state {};
    stencil_face_state.compare = WGPUCompareFunction::WGPUCompareFunction_Always;
    stencil_face_state.depthFailOp = WGPUStencilOperation::WGPUStencilOperation_Keep;
    stencil_face_state.failOp = WGPUStencilOperation::WGPUStencilOperation_Keep;
    stencil_face_state.passOp = WGPUStencilOperation::WGPUStencilOperation_Keep;

    WGPUDepthStencilState depth_stencil_state {};
    depth_stencil_state.depthCompare = WGPUCompareFunction::WGPUCompareFunction_Less;
    depth_stencil_state.depthWriteEnabled = true;
    depth_stencil_state.stencilReadMask = 0;
    depth_stencil_state.stencilWriteMask = 0;
    depth_stencil_state.stencilFront = stencil_face_state;
    depth_stencil_state.stencilBack = stencil_face_state;
    depth_stencil_state.format = depth_texture_format;

    std::vector<WGPUBindGroupLayout> bind_group_layout_handles;
    std::transform(bind_group_layouts.begin(), bind_group_layouts.end(), std::back_insert_iterator(bind_group_layout_handles),
        [](const BindGroupLayout* layout) { return layout->handle(); });

    WGPUPipelineLayoutDescriptor pipeline_layout_desc {};
    pipeline_layout_desc.bindGroupLayoutCount = bind_group_layout_handles.size();
    pipeline_layout_desc.bindGroupLayouts = bind_group_layout_handles.data();
    pipeline_layout_desc.nextInChain = nullptr;

    m_pipeline_layout = std::make_unique<PipelineLayout>(device, pipeline_layout_desc);

    std::vector<WGPUVertexBufferLayout> layouts;
    std::transform(vertex_buffer_infos.begin(), vertex_buffer_infos.end(), std::back_insert_iterator(layouts),
        [](const webgpu_engine::util::SingleVertexBufferInfo& info) { return info.vertex_buffer_layout(); });

    WGPURenderPipelineDescriptor pipeline_desc {};
    pipeline_desc.vertex.module = vertex_shader.handle();
    pipeline_desc.vertex.entryPoint = "vertexMain";
    pipeline_desc.vertex.bufferCount = layouts.size();
    pipeline_desc.vertex.buffers = layouts.data();
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants = nullptr;
    pipeline_desc.primitive.topology = WGPUPrimitiveTopology::WGPUPrimitiveTopology_TriangleStrip;
    pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat::WGPUIndexFormat_Uint16;
    pipeline_desc.primitive.frontFace = WGPUFrontFace::WGPUFrontFace_CCW;
    pipeline_desc.primitive.cullMode = WGPUCullMode::WGPUCullMode_None;
    pipeline_desc.fragment = &fragment_state;
    pipeline_desc.depthStencil = &depth_stencil_state;
    pipeline_desc.multisample.count = 1;
    pipeline_desc.multisample.mask = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;
    pipeline_desc.layout = m_pipeline_layout->handle();

    m_pipeline = std::make_unique<raii::RenderPipeline>(device, pipeline_desc);
}

const RenderPipeline& GenericRenderPipeline::pipeline() const { return *m_pipeline; }

} // namespace webgpu_engine::raii

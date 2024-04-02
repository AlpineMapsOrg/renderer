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

#include "PipelineManager.h"

#include "util/VertexBufferInfo.h"
#include <array>

namespace webgpu_engine {

PipelineManager::PipelineManager(WGPUDevice device, ShaderModuleManager& shader_manager)
    : m_device(device)
    , m_shader_manager(&shader_manager)
{
}

const raii::RenderPipeline& PipelineManager::debug_triangle_pipeline() const { return *m_debug_triangle_pipeline; }
const raii::RenderPipeline& PipelineManager::debug_config_and_camera_pipeline() const { return *m_debug_config_and_camera_pipeline; }
const raii::RenderPipeline& PipelineManager::tile_pipeline() const { return *m_tile_pipeline; }

void PipelineManager::create_pipelines(WGPUTextureFormat color_target_format, WGPUTextureFormat depth_texture_format,
    const raii::BindGroupWithLayout& bind_group_info, const raii::BindGroupWithLayout& shared_config_bind_group,
    const raii::BindGroupWithLayout& camera_bind_group, const raii::BindGroupWithLayout& tile_bind_group)
{
    create_debug_pipeline(color_target_format);
    create_debug_config_and_camera_pipeline(color_target_format, depth_texture_format, bind_group_info);
    create_tile_pipeline(color_target_format, depth_texture_format, shared_config_bind_group, camera_bind_group, tile_bind_group);
    m_pipelines_created = true;
}

void PipelineManager::release_pipelines()
{
    m_debug_triangle_pipeline.release();
    m_debug_config_and_camera_pipeline.release();
    m_tile_pipeline.release();
    m_pipelines_created = false;
}

bool PipelineManager::pipelines_created() const { return m_pipelines_created; }

void PipelineManager::create_debug_pipeline(WGPUTextureFormat color_target_format)
{
    WGPURenderPipelineDescriptor pipelineDesc {};
    pipelineDesc.vertex.module = m_shader_manager->debug_triangle().handle();
    pipelineDesc.vertex.entryPoint = "vertexMain";
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology::WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat::WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace::WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode::WGPUCullMode_None;

    WGPUFragmentState fragmentState {};
    fragmentState.module = m_shader_manager->debug_triangle().handle();
    fragmentState.entryPoint = "fragmentMain";
    fragmentState.targetCount = 1;

    WGPUColorTargetState colorTargetState {};
    colorTargetState.format = color_target_format;
    fragmentState.targets = &colorTargetState;

    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    WGPUBlendState blendState {};
    blendState.color.srcFactor = WGPUBlendFactor::WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor::WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation::WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor::WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor::WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation::WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget {};
    colorTarget.format = color_target_format;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask::WGPUColorWriteMask_All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    m_debug_triangle_pipeline = std::make_unique<raii::RenderPipeline>(m_device, pipelineDesc);
}

void PipelineManager::create_debug_config_and_camera_pipeline(
    WGPUTextureFormat color_target_format, WGPUTextureFormat depth_texture_format, const raii::BindGroupWithLayout& bind_group_info)
{
    WGPUBlendState blend_state {};
    blend_state.color.srcFactor = WGPUBlendFactor::WGPUBlendFactor_SrcAlpha;
    blend_state.color.dstFactor = WGPUBlendFactor::WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.color.operation = WGPUBlendOperation::WGPUBlendOperation_Add;
    blend_state.alpha.srcFactor = WGPUBlendFactor::WGPUBlendFactor_Zero;
    blend_state.alpha.dstFactor = WGPUBlendFactor::WGPUBlendFactor_One;
    blend_state.alpha.operation = WGPUBlendOperation::WGPUBlendOperation_Add;

    WGPUColorTargetState color_target {};
    color_target.format = color_target_format;
    color_target.blend = &blend_state;
    color_target.writeMask = WGPUColorWriteMask::WGPUColorWriteMask_All;

    WGPUFragmentState fragment_state {};
    fragment_state.module = m_shader_manager->debug_config_and_camera().handle();
    fragment_state.entryPoint = "fragmentMain";
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;
    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target;

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

    const WGPUBindGroupLayout bind_group_layout = bind_group_info.bind_group_layout().handle();
    WGPUPipelineLayoutDescriptor pipeline_layout_desc {};
    pipeline_layout_desc.bindGroupLayouts = &bind_group_layout;
    pipeline_layout_desc.bindGroupLayoutCount = 1;
    pipeline_layout_desc.nextInChain = nullptr;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pipeline_layout_desc);

    WGPURenderPipelineDescriptor pipeline_desc {};
    pipeline_desc.vertex.module = m_shader_manager->debug_config_and_camera().handle();
    pipeline_desc.vertex.entryPoint = "vertexMain";
    pipeline_desc.vertex.bufferCount = 0;
    pipeline_desc.vertex.buffers = nullptr;
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants = nullptr;
    pipeline_desc.primitive.topology = WGPUPrimitiveTopology::WGPUPrimitiveTopology_TriangleList;
    pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat::WGPUIndexFormat_Undefined;
    pipeline_desc.primitive.frontFace = WGPUFrontFace::WGPUFrontFace_CCW;
    pipeline_desc.primitive.cullMode = WGPUCullMode::WGPUCullMode_None;
    pipeline_desc.fragment = &fragment_state;
    pipeline_desc.depthStencil = &depth_stencil_state;
    pipeline_desc.multisample.count = 1;
    pipeline_desc.multisample.mask = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;
    pipeline_desc.layout = layout;

    m_debug_config_and_camera_pipeline = std::make_unique<raii::RenderPipeline>(m_device, pipeline_desc);
}

void PipelineManager::create_tile_pipeline(WGPUTextureFormat color_target_format, WGPUTextureFormat depth_texture_format,
    const raii::BindGroupWithLayout& shared_config_bind_group, const raii::BindGroupWithLayout& camera_bind_group,
    const raii::BindGroupWithLayout& tile_bind_group)
{
    WGPUBlendState blend_state {};
    blend_state.color.srcFactor = WGPUBlendFactor::WGPUBlendFactor_SrcAlpha;
    blend_state.color.dstFactor = WGPUBlendFactor::WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.color.operation = WGPUBlendOperation::WGPUBlendOperation_Add;
    blend_state.alpha.srcFactor = WGPUBlendFactor::WGPUBlendFactor_Zero;
    blend_state.alpha.dstFactor = WGPUBlendFactor::WGPUBlendFactor_One;
    blend_state.alpha.operation = WGPUBlendOperation::WGPUBlendOperation_Add;

    WGPUColorTargetState color_target {};
    color_target.format = color_target_format;
    color_target.blend = &blend_state;
    color_target.writeMask = WGPUColorWriteMask::WGPUColorWriteMask_All;

    WGPUFragmentState fragment_state {};
    fragment_state.module = m_shader_manager->tile().handle();
    fragment_state.entryPoint = "fragmentMain";
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;
    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target;

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

    std::array<WGPUBindGroupLayout, 3> bind_group_layouts = { shared_config_bind_group.bind_group_layout().handle(),
        camera_bind_group.bind_group_layout().handle(), tile_bind_group.bind_group_layout().handle() };
    WGPUPipelineLayoutDescriptor pipeline_layout_desc {};
    pipeline_layout_desc.bindGroupLayouts = bind_group_layouts.data();
    pipeline_layout_desc.bindGroupLayoutCount = 3;
    pipeline_layout_desc.nextInChain = nullptr;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(m_device, &pipeline_layout_desc);

    util::VertexBufferInfo layout_info;
    layout_info.add_buffer(WGPUVertexStepMode_Instance);
    layout_info.add_buffer(WGPUVertexStepMode_Instance);
    layout_info.add_buffer(WGPUVertexStepMode_Instance);
    layout_info.add_buffer(WGPUVertexStepMode_Instance);
    layout_info.get_buffer_info(0).add_attribute<float, 4>(0);
    layout_info.get_buffer_info(1).add_attribute<int32_t, 1>(1);
    layout_info.get_buffer_info(2).add_attribute<int32_t, 1>(2);
    layout_info.get_buffer_info(3).add_attribute<int32_t, 1>(3);
    const auto layouts = layout_info.vertex_buffer_layouts();

    WGPURenderPipelineDescriptor pipeline_desc {};
    pipeline_desc.vertex.module = m_shader_manager->tile().handle();
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
    pipeline_desc.layout = layout;

    m_tile_pipeline = std::make_unique<raii::RenderPipeline>(m_device, pipeline_desc);
}

void PipelineManager::create_shadow_pipeline() {
    //TODO
}
}

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
#include "ShaderModuleManager.h"

#include "webgpu.hpp"

namespace webgpu_engine {

PipelineManager::PipelineManager(wgpu::Device& device, ShaderModuleManager& shader_manager)
    : m_device(&device)
    , m_shader_manager(&shader_manager)
{

}


wgpu::RenderPipeline PipelineManager::debug_triangle_pipeline() const { return m_debug_triangle_pipeline; }
wgpu::RenderPipeline PipelineManager::debug_config_and_camera_pipeline() const { return m_debug_config_and_camera_pipeline; }

void PipelineManager::create_pipelines(wgpu::TextureFormat color_target_format, BindGroupInfo& bind_group_info)
{
    create_debug_pipeline(color_target_format);
    create_debug_config_and_camera_pipeline(color_target_format, bind_group_info);
}

void PipelineManager::release_pipelines()
{
    m_debug_triangle_pipeline.release();
    m_debug_triangle_pipeline = nullptr;
    m_debug_config_and_camera_pipeline.release();
    m_debug_config_and_camera_pipeline = nullptr;
}

bool PipelineManager::pipelines_created()
{
    return m_debug_triangle_pipeline != nullptr && m_debug_config_and_camera_pipeline != nullptr;
}

void PipelineManager::create_debug_pipeline(wgpu::TextureFormat color_target_format) {
    wgpu::RenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.vertex.module = m_shader_manager->debug_triangle();
    pipelineDesc.vertex.entryPoint = "vertexMain";
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

    wgpu::FragmentState fragmentState{};
    fragmentState.module = m_shader_manager->debug_triangle();
    fragmentState.entryPoint = "fragmentMain";
    fragmentState.targetCount = 1;

    wgpu::ColorTargetState colorTargetState{};
    colorTargetState.format = color_target_format;
    fragmentState.targets = &colorTargetState;

    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colorTarget;
    colorTarget.format = color_target_format;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    m_debug_triangle_pipeline = m_device->createRenderPipeline(pipelineDesc);
}

void PipelineManager::create_debug_config_and_camera_pipeline(wgpu::TextureFormat color_target_format, BindGroupInfo& bind_group_info) {

    wgpu::RenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.vertex.module = m_shader_manager->debug_config_and_camera();
    pipelineDesc.vertex.entryPoint = "vertexMain";
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

    wgpu::FragmentState fragmentState{};
    fragmentState.module = m_shader_manager->debug_config_and_camera();
    fragmentState.entryPoint = "fragmentMain";
    fragmentState.targetCount = 1;

    wgpu::ColorTargetState colorTargetState{};
    colorTargetState.format = color_target_format;
    fragmentState.targets = &colorTargetState;

    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colorTarget;
    colorTarget.format = color_target_format;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*) &bind_group_info.m_bind_group_layout;
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.nextInChain = nullptr;
    wgpu::PipelineLayout layout = m_device->createPipelineLayout(pipelineLayoutDesc);

    pipelineDesc.layout = layout;


    m_debug_config_and_camera_pipeline = m_device->createRenderPipeline(pipelineDesc);
}

void PipelineManager::create_tile_pipeline() {
    //TODO
}
void PipelineManager::create_shadow_pipeline() {
    //TODO
}

void BindGroupInfo::create_bind_group_layout(wgpu::Device& device)
{
    wgpu::BindGroupLayoutDescriptor desc{};
    desc.entries = m_bind_group_layout_entries.data();
    desc.entryCount = m_bind_group_layout_entries.size();
    desc.nextInChain = nullptr;
    desc.label = "BindGroupInfo - BindGroupLayout";
    m_bind_group_layout = device.createBindGroupLayout(desc);
}

void BindGroupInfo::create_bind_group(wgpu::Device& device)
{
    wgpu::BindGroupDescriptor desc{};
    desc.layout = m_bind_group_layout;
    desc.entries = m_bind_group_entries.data();
    desc.entryCount = m_bind_group_entries.size();
    desc.nextInChain = nullptr;
    desc.label = "BindGroupInfo - BindGroup";
    m_bind_group = device.createBindGroup(desc);
}

void BindGroupInfo::init(wgpu::Device& device)
{
    create_bind_group_layout(device);
    create_bind_group(device);
}

void BindGroupInfo::bind(wgpu::RenderPassEncoder& render_pass, uint32_t group_index)
{
    render_pass.setBindGroup(group_index, m_bind_group, 0, nullptr);
}



}

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

#include "raii/BindGroupLayout.h"
#include "raii/Pipeline.h"
#include "util/VertexBufferInfo.h"

namespace webgpu_engine {

PipelineManager::PipelineManager(WGPUDevice device, ShaderModuleManager& shader_manager)
    : m_device(device)
    , m_shader_manager(&shader_manager)
{
}

const raii::GenericRenderPipeline& PipelineManager::tile_pipeline() const { return *m_tile_pipeline; }

const raii::GenericRenderPipeline& PipelineManager::compose_pipeline() const { return *m_compose_pipeline; }

const raii::GenericRenderPipeline& PipelineManager::atmosphere_pipeline() const { return *m_atmosphere_pipeline; }

const raii::BindGroupLayout& PipelineManager::shared_config_bind_group_layout() const { return *m_shared_config_bind_group_layout; }

const raii::BindGroupLayout& PipelineManager::camera_bind_group_layout() const { return *m_camera_bind_group_layout; }

const raii::BindGroupLayout& PipelineManager::tile_bind_group_layout() const { return *m_tile_bind_group_layout; }

const raii::BindGroupLayout& PipelineManager::compose_bind_group_layout() const { return *m_compose_bind_group_layout; }

void PipelineManager::create_pipelines()
{
    create_bind_group_layouts();
    create_tile_pipeline();
    create_compose_pipeline();
    create_atmosphere_pipeline();
    m_pipelines_created = true;
}

void PipelineManager::create_bind_group_layouts()
{
    WGPUBindGroupLayoutEntry shared_config_entry {};
    shared_config_entry.binding = 0;
    shared_config_entry.visibility = WGPUShaderStage::WGPUShaderStage_Vertex | WGPUShaderStage::WGPUShaderStage_Fragment;
    shared_config_entry.buffer.type = WGPUBufferBindingType_Uniform;
    shared_config_entry.buffer.minBindingSize = 0;
    m_shared_config_bind_group_layout
        = std::make_unique<raii::BindGroupLayout>(m_device, std::vector<WGPUBindGroupLayoutEntry> { shared_config_entry }, "shared config bind group layout");

    WGPUBindGroupLayoutEntry camera_entry {};
    camera_entry.binding = 0;
    camera_entry.visibility = WGPUShaderStage::WGPUShaderStage_Vertex | WGPUShaderStage::WGPUShaderStage_Fragment;
    camera_entry.buffer.type = WGPUBufferBindingType_Uniform;
    camera_entry.buffer.minBindingSize = 0;
    m_camera_bind_group_layout
        = std::make_unique<raii::BindGroupLayout>(m_device, std::vector<WGPUBindGroupLayoutEntry> { camera_entry }, "camera bind group layout");

    WGPUBindGroupLayoutEntry n_vertices_entry {};
    n_vertices_entry.binding = 0;
    n_vertices_entry.visibility = WGPUShaderStage::WGPUShaderStage_Vertex;
    n_vertices_entry.buffer.type = WGPUBufferBindingType_Uniform;
    n_vertices_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry heightmap_texture_entry {};
    heightmap_texture_entry.binding = 1;
    heightmap_texture_entry.visibility = WGPUShaderStage_Vertex;
    heightmap_texture_entry.texture.sampleType = WGPUTextureSampleType_Uint;
    heightmap_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;

    WGPUBindGroupLayoutEntry heightmap_texture_sampler {};
    heightmap_texture_sampler.binding = 2;
    heightmap_texture_sampler.visibility = WGPUShaderStage_Vertex;
    heightmap_texture_sampler.sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutEntry ortho_texture_entry {};
    ortho_texture_entry.binding = 3;
    ortho_texture_entry.visibility = WGPUShaderStage_Fragment;
    ortho_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    ortho_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;

    WGPUBindGroupLayoutEntry ortho_texture_sampler {};
    ortho_texture_sampler.binding = 4;
    ortho_texture_sampler.visibility = WGPUShaderStage_Fragment;
    ortho_texture_sampler.sampler.type = WGPUSamplerBindingType_Filtering;

    m_tile_bind_group_layout = std::make_unique<raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            n_vertices_entry, heightmap_texture_entry, heightmap_texture_sampler, ortho_texture_entry, ortho_texture_sampler },
        "tile bind group");

    WGPUBindGroupLayoutEntry albedo_texture_entry {};
    albedo_texture_entry.binding = 0;
    albedo_texture_entry.visibility = WGPUShaderStage_Fragment;
    albedo_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    albedo_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry position_texture_entry {};
    position_texture_entry.binding = 1;
    position_texture_entry.visibility = WGPUShaderStage_Fragment;
    position_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    position_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry normal_texture_entry {};
    normal_texture_entry.binding = 2;
    normal_texture_entry.visibility = WGPUShaderStage_Fragment;
    normal_texture_entry.texture.sampleType = WGPUTextureSampleType_Uint;
    normal_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry atmosphere_texture_entry {};
    atmosphere_texture_entry.binding = 3;
    atmosphere_texture_entry.visibility = WGPUShaderStage_Fragment;
    atmosphere_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    atmosphere_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry compose_sampler_entry {};
    compose_sampler_entry.binding = 4;
    compose_sampler_entry.visibility = WGPUShaderStage_Fragment;
    compose_sampler_entry.sampler.type = WGPUSamplerBindingType_Filtering;

    m_compose_bind_group_layout = std::make_unique<raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            albedo_texture_entry, position_texture_entry, normal_texture_entry, atmosphere_texture_entry, compose_sampler_entry },
        "compose bind group layout");
}

void PipelineManager::release_pipelines()
{
    m_tile_pipeline.release();
    m_compose_pipeline.release();
    m_atmosphere_pipeline.release();
    m_pipelines_created = false;
}

bool PipelineManager::pipelines_created() const { return m_pipelines_created; }

void PipelineManager::create_tile_pipeline()
{
    util::SingleVertexBufferInfo bounds_buffer_info(WGPUVertexStepMode_Instance);
    bounds_buffer_info.add_attribute<float, 4>(0);
    util::SingleVertexBufferInfo texture_layer_buffer_info(WGPUVertexStepMode_Instance);
    texture_layer_buffer_info.add_attribute<int32_t, 1>(1);
    util::SingleVertexBufferInfo tileset_id_buffer_info(WGPUVertexStepMode_Instance);
    tileset_id_buffer_info.add_attribute<int32_t, 1>(2);
    util::SingleVertexBufferInfo zoomlevel_buffer_info(WGPUVertexStepMode_Instance);
    zoomlevel_buffer_info.add_attribute<int32_t, 1>(3);

    FramebufferFormat format {};
    format.depth_format = WGPUTextureFormat_Depth24Plus;
    format.color_formats.emplace_back(WGPUTextureFormat_RGBA8Unorm);
    format.color_formats.emplace_back(WGPUTextureFormat_RGBA8Unorm);
    format.color_formats.emplace_back(WGPUTextureFormat_RG8Uint);
    format.color_formats.emplace_back(WGPUTextureFormat_RGBA8Unorm);

    m_tile_pipeline = std::make_unique<raii::GenericRenderPipeline>(m_device, m_shader_manager->tile(), m_shader_manager->tile(),
        std::vector<util::SingleVertexBufferInfo> { bounds_buffer_info, texture_layer_buffer_info, tileset_id_buffer_info, zoomlevel_buffer_info }, format,
        std::vector<const raii::BindGroupLayout*> {
            m_shared_config_bind_group_layout.get(), m_camera_bind_group_layout.get(), m_tile_bind_group_layout.get() });
}

void PipelineManager::create_compose_pipeline()
{
    FramebufferFormat format {};
    format.depth_format = WGPUTextureFormat_Depth24Plus;
    format.color_formats.emplace_back(WGPUTextureFormat_BGRA8Unorm);

    m_compose_pipeline = std::make_unique<raii::GenericRenderPipeline>(m_device, m_shader_manager->screen_pass_vert(), m_shader_manager->compose_frag(),
        std::vector<util::SingleVertexBufferInfo> {}, format,
        std::vector<const raii::BindGroupLayout*> {
            m_shared_config_bind_group_layout.get(), m_camera_bind_group_layout.get(), m_compose_bind_group_layout.get() });
}

void PipelineManager::create_atmosphere_pipeline()
{
    FramebufferFormat format {};
    format.depth_format = WGPUTextureFormat_Depth24Plus;
    format.color_formats.emplace_back(WGPUTextureFormat_RGBA8Unorm);

    m_atmosphere_pipeline = std::make_unique<raii::GenericRenderPipeline>(m_device, m_shader_manager->screen_pass_vert(), m_shader_manager->atmosphere_frag(),
        std::vector<util::SingleVertexBufferInfo> {}, format, std::vector<const raii::BindGroupLayout*> { m_camera_bind_group_layout.get() });
}

void PipelineManager::create_shadow_pipeline() {
    //TODO
}
}

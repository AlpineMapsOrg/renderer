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

#include "PipelineManager.h"

#include <webgpu/raii/BindGroupLayout.h>
#include <webgpu/raii/Pipeline.h>
#include <webgpu/util/VertexBufferInfo.h>

namespace webgpu_engine {

PipelineManager::PipelineManager(WGPUDevice device, const ShaderModuleManager& shader_manager)
    : m_device(device)
    , m_shader_manager(&shader_manager)
{
}

const webgpu::raii::GenericRenderPipeline& PipelineManager::render_tiles_pipeline() const { return *m_render_tiles_pipeline; }

const webgpu::raii::GenericRenderPipeline& PipelineManager::render_atmosphere_pipeline() const { return *m_render_atmosphere_pipeline; }

const webgpu::raii::RenderPipeline& PipelineManager::render_lines_pipeline() const { return *m_render_lines_pipeline; }

const webgpu::raii::GenericRenderPipeline& PipelineManager::compose_pipeline() const { return *m_compose_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::normals_compute_pipeline() const { return *m_normals_compute_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::snow_compute_pipeline() const { return *m_snow_compute_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::downsample_compute_pipeline() const { return *m_downsample_compute_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::upsample_textures_compute_pipeline() const { return *m_upsample_textures_compute_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::avalanche_trajectories_compute_pipeline() const
{
    return *m_avalanche_trajectories_compute_pipeline;
}

const webgpu::raii::CombinedComputePipeline& PipelineManager::buffer_to_texture_compute_pipeline() const
{
    return *m_avalanche_trajectories_buffer_to_texture_compute_pipeline;
}

const webgpu::raii::CombinedComputePipeline& PipelineManager::avalanche_influence_area_compute_pipeline() const
{
    return *m_avalanche_influence_area_compute_pipeline;
}

const webgpu::raii::CombinedComputePipeline& PipelineManager::d8_compute_pipeline() const { return *m_d8_compute_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::release_point_compute_pipeline() const { return *m_release_point_compute_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::height_decode_compute_pipeline() const { return *m_height_decode_compute_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::fxaa_compute_pipeline() const { return *m_fxaa_compute_pipeline; }

const webgpu::raii::CombinedComputePipeline& PipelineManager::iterative_simulation_compute_pipeline() const { return *m_iterative_simulation_compute_pipeline; }

const webgpu::raii::BindGroupLayout& PipelineManager::shared_config_bind_group_layout() const { return *m_shared_config_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::camera_bind_group_layout() const { return *m_camera_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::tile_bind_group_layout() const { return *m_tile_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::compose_bind_group_layout() const { return *m_compose_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::normals_compute_bind_group_layout() const { return *m_normals_compute_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::snow_compute_bind_group_layout() const { return *m_snow_compute_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::downsample_compute_bind_group_layout() const { return *m_downsample_compute_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::upsample_textures_compute_bind_group_layout() const
{
    return *m_upsample_textures_compute_bind_group_layout;
}

const webgpu::raii::BindGroupLayout& PipelineManager::lines_bind_group_layout() const { return *m_lines_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::depth_texture_bind_group_layout() const { return *m_depth_texture_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::avalanche_trajectories_bind_group_layout() const { return *m_avalanche_trajectories_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::buffer_to_texture_bind_group_layout() const
{
    return *m_avalanche_trajectories_buffer_to_texture_bind_group_layout;
}

const webgpu::raii::BindGroupLayout& PipelineManager::avalanche_influence_area_bind_group_layout() const
{
    return *m_avalanche_influence_area_bind_group_layout;
}

const webgpu::raii::BindGroupLayout& PipelineManager::d8_compute_bind_group_layout() const { return *m_d8_compute_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::release_point_compute_bind_group_layout() const { return *m_release_point_compute_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::height_decode_compute_bind_group_layout() const { return *m_height_decode_compute_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::fxaa_compute_bind_group_layout() const { return *m_fxaa_compute_bind_group_layout; }

const webgpu::raii::BindGroupLayout& PipelineManager::iterative_simulation_compute_bind_group_layout() const
{
    return *m_iterative_simulation_compute_bind_group_layout;
}
// For MipMap-Creation
const webgpu::raii::CombinedComputePipeline& PipelineManager::mipmap_creation_pipeline() const { return *m_mipmap_creation_compute_pipeline; }
const webgpu::raii::BindGroupLayout& PipelineManager::mipmap_creation_bind_group_layout() const { return *m_mipmap_creation_bind_group_layout; };

void PipelineManager::create_pipelines()
{
    create_bind_group_layouts();

    create_render_tiles_pipeline();
    create_render_atmosphere_pipeline();
    create_render_lines_pipeline();
    create_compose_pipeline();

    create_normals_compute_pipeline();
    create_snow_compute_pipeline();
    create_downsample_compute_pipeline();
    create_upsample_textures_compute_pipeline();
    create_avalanche_trajectories_compute_pipeline();
    create_buffer_to_texture_compute_pipeline();
    create_avalanche_influence_area_compute_pipeline();
    create_d8_compute_pipeline();
    create_release_point_compute_pipeline();
    create_height_decode_compute_pipeline();
    create_mipmap_creation_pipeline();
    create_fxaa_compute_pipeline();
    create_iterative_simulation_compute_pipeline();

    m_pipelines_created = true;
}

void PipelineManager::create_bind_group_layouts()
{
    create_shared_config_bind_group_layout();
    create_camera_bind_group_layout();
    create_tile_bind_group_layout();
    create_compose_bind_group_layout();
    create_normals_compute_bind_group_layout();
    create_snow_compute_bind_group_layout();
    create_downsample_compute_bind_group_layout();
    create_upsample_textures_compute_bind_group_layout();
    create_lines_bind_group_layout();
    create_depth_texture_bind_group_layout();
    create_avalanche_trajectory_bind_group_layout();
    create_buffer_to_texture_bind_group_layout();
    create_avalanche_influence_area_bind_group_layout();
    create_d8_compute_bind_group_layout();
    create_release_points_compute_bind_group_layout();
    create_height_decode_compute_bind_group_layout();
    create_mipmap_creation_bind_group_layout();
    create_fxaa_compute_bind_group_layout();
    create_iterative_simulation_compute_bind_group_layout();
}

void PipelineManager::release_pipelines()
{
    m_render_tiles_pipeline.release();
    m_render_atmosphere_pipeline.release();
    m_render_lines_pipeline.release();
    m_compose_pipeline.release();

    m_normals_compute_pipeline.release();
    m_snow_compute_pipeline.release();
    m_downsample_compute_pipeline.release();
    m_upsample_textures_compute_pipeline.release();
    m_avalanche_trajectories_compute_pipeline.release();
    m_avalanche_trajectories_buffer_to_texture_compute_pipeline.release();
    m_avalanche_influence_area_compute_pipeline.release();
    m_d8_compute_pipeline.release();
    m_release_point_compute_pipeline.release();
    m_height_decode_compute_pipeline.release();
    m_fxaa_compute_pipeline.release();
    m_iterative_simulation_compute_pipeline.release();

    m_mipmap_creation_compute_pipeline.release();

    m_pipelines_created = false;
}

bool PipelineManager::pipelines_created() const { return m_pipelines_created; }

void PipelineManager::create_render_tiles_pipeline()
{
    webgpu::util::SingleVertexBufferInfo bounds_buffer_info(WGPUVertexStepMode_Instance);
    bounds_buffer_info.add_attribute<float, 4>(0);
    webgpu::util::SingleVertexBufferInfo texture_layer_buffer_info(WGPUVertexStepMode_Instance);
    texture_layer_buffer_info.add_attribute<int32_t, 1>(1);
    webgpu::util::SingleVertexBufferInfo ortho_texture_layer_buffer_info(WGPUVertexStepMode_Instance);
    ortho_texture_layer_buffer_info.add_attribute<int32_t, 1>(2);
    webgpu::util::SingleVertexBufferInfo tileset_id_buffer_info(WGPUVertexStepMode_Instance);
    tileset_id_buffer_info.add_attribute<int32_t, 1>(3);
    webgpu::util::SingleVertexBufferInfo height_zoomlevel_buffer_info(WGPUVertexStepMode_Instance);
    height_zoomlevel_buffer_info.add_attribute<int32_t, 1>(4);
    webgpu::util::SingleVertexBufferInfo tile_id_buffer_info(WGPUVertexStepMode_Instance);
    tile_id_buffer_info.add_attribute<uint32_t, 4>(5);
    webgpu::util::SingleVertexBufferInfo ortho_zoomlevel_buffer_info(WGPUVertexStepMode_Instance);
    ortho_zoomlevel_buffer_info.add_attribute<int32_t, 1>(6);

    webgpu::FramebufferFormat format {};
    format.depth_format = WGPUTextureFormat_Depth24Plus;
    format.color_formats.emplace_back(WGPUTextureFormat_R32Uint); // albedo
    format.color_formats.emplace_back(WGPUTextureFormat_RGBA32Float); // position
    format.color_formats.emplace_back(WGPUTextureFormat_RG16Uint); // normal
    format.color_formats.emplace_back(WGPUTextureFormat_R32Uint); // overlay

    m_render_tiles_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(m_device,
        m_shader_manager->render_tiles(),
        m_shader_manager->render_tiles(),
        std::vector<webgpu::util::SingleVertexBufferInfo> {
            bounds_buffer_info,
            texture_layer_buffer_info,
            ortho_texture_layer_buffer_info,
            tileset_id_buffer_info,
            height_zoomlevel_buffer_info,
            tile_id_buffer_info,
            ortho_zoomlevel_buffer_info,
        },
        format,
        std::vector<const webgpu::raii::BindGroupLayout*> {
            m_shared_config_bind_group_layout.get(),
            m_camera_bind_group_layout.get(),
            m_tile_bind_group_layout.get(),
        });
}

void PipelineManager::create_render_atmosphere_pipeline()
{
    webgpu::FramebufferFormat format {};
    format.depth_format = WGPUTextureFormat_Undefined;  // no depth buffer needed
    format.color_formats.emplace_back(WGPUTextureFormat_RGBA8Unorm);

    m_render_atmosphere_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(m_device, m_shader_manager->render_atmosphere(),
        m_shader_manager->render_atmosphere(), std::vector<webgpu::util::SingleVertexBufferInfo> {}, format,
        std::vector<const webgpu::raii::BindGroupLayout*> { m_camera_bind_group_layout.get() });
}

void PipelineManager::create_render_lines_pipeline()
{
    WGPUBlendState blend_state {};
    blend_state.color.operation = WGPUBlendOperation_Add;
    blend_state.color.srcFactor = WGPUBlendFactor_One;
    blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.alpha.operation = WGPUBlendOperation_Add;
    blend_state.alpha.srcFactor = WGPUBlendFactor_Zero;
    blend_state.alpha.dstFactor = WGPUBlendFactor_One;

    WGPUColorTargetState color_target_state {};
    color_target_state.blend = &blend_state;
    color_target_state.writeMask = WGPUColorWriteMask_All;
    color_target_state.format = WGPUTextureFormat_BGRA8Unorm;

    WGPUFragmentState fragment_state {};
    fragment_state.module = m_shader_manager->render_lines().handle();
    fragment_state.entryPoint = WGPUStringView { .data = "fragmentMain", .length = WGPU_STRLEN };
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;
    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target_state;

    std::vector<WGPUBindGroupLayout> bind_group_layout_handles { m_shared_config_bind_group_layout->handle(), m_camera_bind_group_layout->handle(),
        m_depth_texture_bind_group_layout->handle(), m_lines_bind_group_layout->handle() };
    webgpu::raii::PipelineLayout layout(m_device, bind_group_layout_handles);

    WGPURenderPipelineDescriptor pipeline_desc {};
    pipeline_desc.label = WGPUStringView { .data = "line render pipeline", .length = WGPU_STRLEN };
    pipeline_desc.vertex.module = m_shader_manager->render_lines().handle();
    pipeline_desc.vertex.entryPoint = WGPUStringView { .data = "vertexMain", .length = WGPU_STRLEN };
    pipeline_desc.vertex.bufferCount = 0;
    pipeline_desc.vertex.buffers = nullptr;
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants = nullptr;
    pipeline_desc.primitive.topology = WGPUPrimitiveTopology::WGPUPrimitiveTopology_LineStrip;
    pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat::WGPUIndexFormat_Uint16;
    pipeline_desc.primitive.frontFace = WGPUFrontFace::WGPUFrontFace_CCW;
    pipeline_desc.primitive.cullMode = WGPUCullMode::WGPUCullMode_None;
    pipeline_desc.fragment = &fragment_state;

    pipeline_desc.depthStencil = nullptr;
    pipeline_desc.multisample.count = 1;
    pipeline_desc.multisample.mask = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;
    pipeline_desc.layout = layout.handle();

    m_render_lines_pipeline = std::make_unique<webgpu::raii::RenderPipeline>(m_device, pipeline_desc);
}

void PipelineManager::create_compose_pipeline()
{
    webgpu::FramebufferFormat format {};
    format.depth_format = WGPUTextureFormat_Depth24Plus; // ImGUI needs attached depth buffer
    format.color_formats.emplace_back(WGPUTextureFormat_BGRA8Unorm);

    m_compose_pipeline = std::make_unique<webgpu::raii::GenericRenderPipeline>(m_device, m_shader_manager->compose_pass(), m_shader_manager->compose_pass(),
        std::vector<webgpu::util::SingleVertexBufferInfo> {}, format,
        std::vector<const webgpu::raii::BindGroupLayout*> {
            m_shared_config_bind_group_layout.get(), m_camera_bind_group_layout.get(), m_compose_bind_group_layout.get() });
}

void PipelineManager::create_shadow_pipeline() {
    //TODO
}

void PipelineManager::create_normals_compute_pipeline()
{
    m_normals_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(
        m_device, m_shader_manager->normals_compute(), std::vector<const webgpu::raii::BindGroupLayout*> { m_normals_compute_bind_group_layout.get() });
}

void PipelineManager::create_snow_compute_pipeline()
{
    m_snow_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(
        m_device, m_shader_manager->snow_compute(), std::vector<const webgpu::raii::BindGroupLayout*> { m_snow_compute_bind_group_layout.get() });
}

void PipelineManager::create_downsample_compute_pipeline()
{
    m_downsample_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(
        m_device, m_shader_manager->downsample_compute(), std::vector<const webgpu::raii::BindGroupLayout*> { m_downsample_compute_bind_group_layout.get() });
}

void PipelineManager::create_upsample_textures_compute_pipeline()
{
    m_upsample_textures_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->upsample_textures_compute(),
        std::vector<const webgpu::raii::BindGroupLayout*> { m_upsample_textures_compute_bind_group_layout.get() });
}

void PipelineManager::create_avalanche_trajectories_compute_pipeline()
{
    m_avalanche_trajectories_compute_pipeline
        = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->avalanche_trajectories_compute(),
            std::vector<const webgpu::raii::BindGroupLayout*> { m_avalanche_trajectories_bind_group_layout.get() });
}

void PipelineManager::create_buffer_to_texture_compute_pipeline()
{
    m_avalanche_trajectories_buffer_to_texture_compute_pipeline
        = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->buffer_to_texture_compute(),
            std::vector<const webgpu::raii::BindGroupLayout*> { m_avalanche_trajectories_buffer_to_texture_bind_group_layout.get() });
}

void PipelineManager::create_avalanche_influence_area_compute_pipeline()
{
    m_avalanche_influence_area_compute_pipeline
        = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->avalanche_influence_area_compute(),
            std::vector<const webgpu::raii::BindGroupLayout*> { m_avalanche_influence_area_bind_group_layout.get() }, "avalanche influence area");
}

void PipelineManager::create_d8_compute_pipeline()
{
    m_d8_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->d8_compute(),
        std::vector<const webgpu::raii::BindGroupLayout*> { m_d8_compute_bind_group_layout.get() }, "d8 compute pipeline");
}

void PipelineManager::create_release_point_compute_pipeline()
{
    m_release_point_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->release_point_compute(),
        std::vector<const webgpu::raii::BindGroupLayout*> { m_release_point_compute_bind_group_layout.get() }, "release point compute pipeline");
}

void PipelineManager::create_height_decode_compute_pipeline()
{
    m_height_decode_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->height_decode_compute(),
        std::vector<const webgpu::raii::BindGroupLayout*> { m_height_decode_compute_bind_group_layout.get() }, "height decode compute pipeline");
}

void PipelineManager::create_mipmap_creation_pipeline()
{
    m_mipmap_creation_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->mipmap_creation_compute(),
        std::vector<const webgpu::raii::BindGroupLayout*> { m_mipmap_creation_bind_group_layout.get() }, "mipmap creation compute pipeline");
        }
void PipelineManager::create_fxaa_compute_pipeline()
{
    m_fxaa_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device, m_shader_manager->fxaa_compute(),
        std::vector<const webgpu::raii::BindGroupLayout*> { m_fxaa_compute_bind_group_layout.get() }, "fxaa compute pipeline");
}

void PipelineManager::create_iterative_simulation_compute_pipeline()
{
    m_iterative_simulation_compute_pipeline = std::make_unique<webgpu::raii::CombinedComputePipeline>(m_device,
        m_shader_manager->iterative_simulation_compute(),
        std::vector<const webgpu::raii::BindGroupLayout*> { m_iterative_simulation_compute_bind_group_layout.get() }, "iterative simulation compute pipeline");
}

void PipelineManager::create_shared_config_bind_group_layout()
{
    WGPUBindGroupLayoutEntry shared_config_entry {};
    shared_config_entry.binding = 0;
    shared_config_entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    shared_config_entry.buffer.type = WGPUBufferBindingType_Uniform;
    shared_config_entry.buffer.minBindingSize = 0;
    m_shared_config_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(
        m_device, std::vector<WGPUBindGroupLayoutEntry> { shared_config_entry }, "shared config bind group layout");
}

void PipelineManager::create_camera_bind_group_layout()
{
    WGPUBindGroupLayoutEntry camera_entry {};
    camera_entry.binding = 0;
    camera_entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    camera_entry.buffer.type = WGPUBufferBindingType_Uniform;
    camera_entry.buffer.minBindingSize = 0;
    m_camera_bind_group_layout
        = std::make_unique<webgpu::raii::BindGroupLayout>(m_device, std::vector<WGPUBindGroupLayoutEntry> { camera_entry }, "camera bind group layout");
}

void PipelineManager::create_tile_bind_group_layout()
{
    WGPUBindGroupLayoutEntry n_vertices_entry {};
    n_vertices_entry.binding = 0;
    n_vertices_entry.visibility = WGPUShaderStage_Vertex;
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
    heightmap_texture_sampler.sampler.type = WGPUSamplerBindingType_NonFiltering;

    WGPUBindGroupLayoutEntry ortho_texture_entry {};
    ortho_texture_entry.binding = 3;
    ortho_texture_entry.visibility = WGPUShaderStage_Fragment;
    ortho_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    ortho_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;

    WGPUBindGroupLayoutEntry ortho_texture_sampler {};
    ortho_texture_sampler.binding = 4;
    ortho_texture_sampler.visibility = WGPUShaderStage_Fragment;
    ortho_texture_sampler.sampler.type = WGPUSamplerBindingType_Filtering;

    m_tile_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            n_vertices_entry, heightmap_texture_entry, heightmap_texture_sampler, ortho_texture_entry, ortho_texture_sampler },
        "tile bind group");
}

void PipelineManager::create_compose_bind_group_layout()
{
    WGPUBindGroupLayoutEntry albedo_texture_entry {};
    albedo_texture_entry.binding = 0;
    albedo_texture_entry.visibility = WGPUShaderStage_Fragment;
    albedo_texture_entry.texture.sampleType = WGPUTextureSampleType_Uint;
    albedo_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry position_texture_entry {};
    position_texture_entry.binding = 1;
    position_texture_entry.visibility = WGPUShaderStage_Fragment;
    position_texture_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
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

    WGPUBindGroupLayoutEntry overlay_texture_entry {};
    overlay_texture_entry.binding = 4;
    overlay_texture_entry.visibility = WGPUShaderStage_Fragment;
    overlay_texture_entry.texture.sampleType = WGPUTextureSampleType_Uint;
    overlay_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry image_overlay_region_entry {};
    image_overlay_region_entry.binding = 5;
    image_overlay_region_entry.visibility = WGPUShaderStage_Fragment;
    image_overlay_region_entry.buffer.type = WGPUBufferBindingType_Uniform;

    WGPUBindGroupLayoutEntry image_overlay_texture_entry {};
    image_overlay_texture_entry.binding = 6;
    image_overlay_texture_entry.visibility = WGPUShaderStage_Fragment;
    image_overlay_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    image_overlay_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry image_overlay_texture_sampler_entry {};
    image_overlay_texture_sampler_entry.binding = 7;
    image_overlay_texture_sampler_entry.visibility = WGPUShaderStage_Fragment;
    image_overlay_texture_sampler_entry.sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutEntry compute_overlay_region_entry {};
    compute_overlay_region_entry.binding = 8;
    compute_overlay_region_entry.visibility = WGPUShaderStage_Fragment;
    compute_overlay_region_entry.buffer.type = WGPUBufferBindingType_Uniform;

    WGPUBindGroupLayoutEntry compute_overlay_texture_entry {};
    compute_overlay_texture_entry.binding = 9;
    compute_overlay_texture_entry.visibility = WGPUShaderStage_Fragment;
    compute_overlay_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    compute_overlay_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry compute_overlay_texture_sampler_entry {};
    compute_overlay_texture_sampler_entry.binding = 10;
    compute_overlay_texture_sampler_entry.visibility = WGPUShaderStage_Fragment;
    compute_overlay_texture_sampler_entry.sampler.type = WGPUSamplerBindingType_Filtering;

    m_compose_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            albedo_texture_entry,
            position_texture_entry,
            normal_texture_entry,
            atmosphere_texture_entry,
            overlay_texture_entry,
            image_overlay_region_entry,
            image_overlay_texture_entry,
            image_overlay_texture_sampler_entry,
            compute_overlay_region_entry,
            compute_overlay_texture_entry,
            compute_overlay_texture_sampler_entry,
        },
        "compose bind group layout");
}

void PipelineManager::create_normals_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_tile_ids_entry {};
    input_tile_ids_entry.binding = 0;
    input_tile_ids_entry.visibility = WGPUShaderStage_Compute;
    input_tile_ids_entry.buffer.type = WGPUBufferBindingType_Uniform;
    input_tile_ids_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_height_texture_entry {};
    input_height_texture_entry.binding = 1;
    input_height_texture_entry.visibility = WGPUShaderStage_Compute;
    input_height_texture_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    input_height_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry output_normal_texture_entry {};
    output_normal_texture_entry.binding = 2;
    output_normal_texture_entry.visibility = WGPUShaderStage_Compute;
    output_normal_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    output_normal_texture_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_normal_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_normals_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            input_tile_ids_entry,
            input_height_texture_entry,
            output_normal_texture_entry,
        },
        "normals compute bind group layout");
}

void PipelineManager::create_snow_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_settings_entry {};
    input_settings_entry.binding = 0;
    input_settings_entry.visibility = WGPUShaderStage_Compute;
    input_settings_entry.buffer.type = WGPUBufferBindingType_Uniform;
    input_settings_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_region_bounds_entry {};
    input_region_bounds_entry.binding = 1;
    input_region_bounds_entry.visibility = WGPUShaderStage_Compute;
    input_region_bounds_entry.buffer.type = WGPUBufferBindingType_Uniform;
    input_region_bounds_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_normal_texture_entry {};
    input_normal_texture_entry.binding = 2;
    input_normal_texture_entry.visibility = WGPUShaderStage_Compute;
    input_normal_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    input_normal_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry input_height_texture_entry {};
    input_height_texture_entry.binding = 3;
    input_height_texture_entry.visibility = WGPUShaderStage_Compute;
    input_height_texture_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    input_height_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry output_snow_texture_entry {};
    output_snow_texture_entry.binding = 4;
    output_snow_texture_entry.visibility = WGPUShaderStage_Compute;
    output_snow_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    output_snow_texture_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_snow_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_snow_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            input_settings_entry,
            input_region_bounds_entry,
            input_height_texture_entry,
            input_normal_texture_entry,
            output_snow_texture_entry,
        },
        "snow compute bind group layout");
}

void PipelineManager::create_downsample_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry downsample_compute_input_tile_ids_entry {};
    downsample_compute_input_tile_ids_entry.binding = 0;
    downsample_compute_input_tile_ids_entry.visibility = WGPUShaderStage_Compute;
    downsample_compute_input_tile_ids_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    downsample_compute_input_tile_ids_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry downsample_compute_key_buffer_entry {};
    downsample_compute_key_buffer_entry.binding = 1;
    downsample_compute_key_buffer_entry.visibility = WGPUShaderStage_Compute;
    downsample_compute_key_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    downsample_compute_key_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry downsample_compute_value_buffer_entry {};
    downsample_compute_value_buffer_entry.binding = 2;
    downsample_compute_value_buffer_entry.visibility = WGPUShaderStage_Compute;
    downsample_compute_value_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    downsample_compute_value_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry downsample_compute_input_textures_entry {};
    downsample_compute_input_textures_entry.binding = 3;
    downsample_compute_input_textures_entry.visibility = WGPUShaderStage_Compute;
    downsample_compute_input_textures_entry.texture.sampleType = WGPUTextureSampleType_Float;
    downsample_compute_input_textures_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;

    WGPUBindGroupLayoutEntry downsample_compute_output_textures_entry {};
    downsample_compute_output_textures_entry.binding = 4;
    downsample_compute_output_textures_entry.visibility = WGPUShaderStage_Compute;
    downsample_compute_output_textures_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2DArray;
    downsample_compute_output_textures_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    downsample_compute_output_textures_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_downsample_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> { downsample_compute_input_tile_ids_entry, downsample_compute_key_buffer_entry,
            downsample_compute_value_buffer_entry, downsample_compute_input_textures_entry, downsample_compute_output_textures_entry },
        "compute: downsample bind group layout");
}

void PipelineManager::create_upsample_textures_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_indices_entry {};
    input_indices_entry.binding = 0;
    input_indices_entry.visibility = WGPUShaderStage_Compute;
    input_indices_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_indices_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_texture_array_entry {};
    input_texture_array_entry.binding = 1;
    input_texture_array_entry.visibility = WGPUShaderStage_Compute;
    input_texture_array_entry.texture.sampleType = WGPUTextureSampleType_Float;
    input_texture_array_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;

    WGPUBindGroupLayoutEntry input_texture_array_sampler {};
    input_texture_array_sampler.binding = 2;
    input_texture_array_sampler.visibility = WGPUShaderStage_Compute;
    input_texture_array_sampler.sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutEntry output_texture_array_entry {};
    output_texture_array_entry.binding = 3;
    output_texture_array_entry.visibility = WGPUShaderStage_Compute;
    output_texture_array_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2DArray;
    output_texture_array_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_texture_array_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_upsample_textures_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> { input_indices_entry, input_texture_array_entry, input_texture_array_sampler, output_texture_array_entry },
        "compute: upsample textures bind group layout");
}

void PipelineManager::create_lines_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_positions_entry {};
    input_positions_entry.binding = 0;
    input_positions_entry.visibility = WGPUShaderStage_Vertex;
    input_positions_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_positions_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_config_entry {};
    input_config_entry.binding = 1;
    input_config_entry.visibility = WGPUShaderStage_Fragment;
    input_config_entry.buffer.type = WGPUBufferBindingType_Uniform;
    input_config_entry.buffer.minBindingSize = 0;

    m_lines_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(
        m_device, std::vector<WGPUBindGroupLayoutEntry> { input_positions_entry, input_config_entry }, "line renderer, bind group layout");
}

void PipelineManager::create_depth_texture_bind_group_layout()
{
    WGPUBindGroupLayoutEntry depth_texture_entry {};
    depth_texture_entry.binding = 0;
    depth_texture_entry.visibility = WGPUShaderStage_Fragment;
    depth_texture_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    depth_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    m_depth_texture_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(
        m_device, std::vector<WGPUBindGroupLayoutEntry> { depth_texture_entry }, "depth texture bind group layout");
}

void PipelineManager::create_avalanche_trajectory_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_settings {};
    input_settings.binding = 0;
    input_settings.visibility = WGPUShaderStage_Compute;
    input_settings.buffer.type = WGPUBufferBindingType_Uniform;
    input_settings.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_normal_texture_entry {};
    input_normal_texture_entry.binding = 1;
    input_normal_texture_entry.visibility = WGPUShaderStage_Compute;
    input_normal_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    input_normal_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry input_height_texture_entry {};
    input_height_texture_entry.binding = 2;
    input_height_texture_entry.visibility = WGPUShaderStage_Compute;
    input_height_texture_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    input_height_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry input_release_points_texture_entry {};
    input_release_points_texture_entry.binding = 3;
    input_release_points_texture_entry.visibility = WGPUShaderStage_Compute;
    input_release_points_texture_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    input_release_points_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry input_normal_sampler {};
    input_normal_sampler.binding = 4;
    input_normal_sampler.visibility = WGPUShaderStage_Compute;
    input_normal_sampler.sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutEntry input_height_sampler {};
    input_height_sampler.binding = 5;
    input_height_sampler.visibility = WGPUShaderStage_Compute;
    input_height_sampler.sampler.type = WGPUSamplerBindingType_NonFiltering;

    WGPUBindGroupLayoutEntry output_storage_buffer_entry {};
    output_storage_buffer_entry.binding = 6;
    output_storage_buffer_entry.visibility = WGPUShaderStage_Compute;
    output_storage_buffer_entry.buffer.type = WGPUBufferBindingType_Storage;
    output_storage_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_layer1_buffer_entry {};
    output_layer1_buffer_entry.binding = 7;
    output_layer1_buffer_entry.visibility = WGPUShaderStage_Compute;
    output_layer1_buffer_entry.buffer.type = WGPUBufferBindingType_Storage;
    output_layer1_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_layer2_buffer_entry {};
    output_layer2_buffer_entry.binding = 8;
    output_layer2_buffer_entry.visibility = WGPUShaderStage_Compute;
    output_layer2_buffer_entry.buffer.type = WGPUBufferBindingType_Storage;
    output_layer2_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_layer3_buffer_entry {};
    output_layer3_buffer_entry.binding = 9;
    output_layer3_buffer_entry.visibility = WGPUShaderStage_Compute;
    output_layer3_buffer_entry.buffer.type = WGPUBufferBindingType_Storage;
    output_layer3_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_layer4_buffer_entry {};
    output_layer4_buffer_entry.binding = 10;
    output_layer4_buffer_entry.visibility = WGPUShaderStage_Compute;
    output_layer4_buffer_entry.buffer.type = WGPUBufferBindingType_Storage;
    output_layer4_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_layer5_buffer_entry {};
    output_layer5_buffer_entry.binding = 11;
    output_layer5_buffer_entry.visibility = WGPUShaderStage_Compute;
    output_layer5_buffer_entry.buffer.type = WGPUBufferBindingType_Storage;
    output_layer5_buffer_entry.buffer.minBindingSize = 0;

    m_avalanche_trajectories_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            input_settings,
            input_normal_texture_entry,
            input_height_texture_entry,
            input_release_points_texture_entry,
            input_normal_sampler,
            input_height_sampler,
            output_storage_buffer_entry,
            output_layer1_buffer_entry,
            output_layer2_buffer_entry,
            output_layer3_buffer_entry,
            output_layer4_buffer_entry,
            output_layer5_buffer_entry,
        },
        "avalanche trajectories compute bind group layout");
}

void PipelineManager::create_buffer_to_texture_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_settings_entry {};
    input_settings_entry.binding = 0;
    input_settings_entry.visibility = WGPUShaderStage_Compute;
    input_settings_entry.buffer.type = WGPUBufferBindingType_Uniform;
    input_settings_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_storage_buffer_entry {};
    input_storage_buffer_entry.binding = 1;
    input_storage_buffer_entry.visibility = WGPUShaderStage_Compute;
    input_storage_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_storage_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_transparency_buffer_entry {};
    input_transparency_buffer_entry.binding = 2;
    input_transparency_buffer_entry.visibility = WGPUShaderStage_Compute;
    input_transparency_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_transparency_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_texture_entry {};
    output_texture_entry.binding = 5;
    output_texture_entry.visibility = WGPUShaderStage_Compute;
    output_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    output_texture_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_avalanche_trajectories_buffer_to_texture_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            input_settings_entry,
            input_storage_buffer_entry,
            input_transparency_buffer_entry,
            output_texture_entry,
        },
        "buffer to texture compute bind group layout");
}

void PipelineManager::create_avalanche_influence_area_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_tile_ids_entry {};
    input_tile_ids_entry.binding = 0;
    input_tile_ids_entry.visibility = WGPUShaderStage_Compute;
    input_tile_ids_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_tile_ids_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_bounds_entry {};
    input_bounds_entry.binding = 1;
    input_bounds_entry.visibility = WGPUShaderStage_Compute;
    input_bounds_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_bounds_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_settings {};
    input_settings.binding = 2;
    input_settings.visibility = WGPUShaderStage_Compute;
    input_settings.buffer.type = WGPUBufferBindingType_Uniform;
    input_settings.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry key_buffer_entry {};
    key_buffer_entry.binding = 3;
    key_buffer_entry.visibility = WGPUShaderStage_Compute;
    key_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    key_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry value_buffer_entry {};
    value_buffer_entry.binding = 4;
    value_buffer_entry.visibility = WGPUShaderStage_Compute;
    value_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    value_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_normal_textures_entry {};
    input_normal_textures_entry.binding = 5;
    input_normal_textures_entry.visibility = WGPUShaderStage_Compute;
    input_normal_textures_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    input_normal_textures_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;

    WGPUBindGroupLayoutEntry input_normal_texture_sampler {};
    input_normal_texture_sampler.binding = 6;
    input_normal_texture_sampler.visibility = WGPUShaderStage_Compute;
    input_normal_texture_sampler.sampler.type = WGPUSamplerBindingType_NonFiltering;

    WGPUBindGroupLayoutEntry input_height_textures_entry {};
    input_height_textures_entry.binding = 7;
    input_height_textures_entry.visibility = WGPUShaderStage_Compute;
    input_height_textures_entry.texture.sampleType = WGPUTextureSampleType_Uint;
    input_height_textures_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;

    WGPUBindGroupLayoutEntry input_height_texture_sampler {};
    input_height_texture_sampler.binding = 8;
    input_height_texture_sampler.visibility = WGPUShaderStage_Compute;
    input_height_texture_sampler.sampler.type = WGPUSamplerBindingType_NonFiltering;

    WGPUBindGroupLayoutEntry output_key_buffer_entry {};
    output_key_buffer_entry.binding = 9;
    output_key_buffer_entry.visibility = WGPUShaderStage_Compute;
    output_key_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    output_key_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_value_buffer_entry {};
    output_value_buffer_entry.binding = 10;
    output_value_buffer_entry.visibility = WGPUShaderStage_Compute;
    output_value_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    output_value_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_tiles_entry {};
    output_tiles_entry.binding = 11;
    output_tiles_entry.visibility = WGPUShaderStage_Compute;
    output_tiles_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2DArray;
    output_tiles_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_tiles_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_avalanche_influence_area_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> { input_tile_ids_entry, input_bounds_entry, input_settings, key_buffer_entry, value_buffer_entry,
            input_normal_textures_entry, input_normal_texture_sampler, input_height_textures_entry, input_height_texture_sampler, output_key_buffer_entry,
            output_value_buffer_entry, output_tiles_entry },
        "avalanche influence area bind group layout");
}

void PipelineManager::create_d8_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_tile_ids_entry {};
    input_tile_ids_entry.binding = 0;
    input_tile_ids_entry.visibility = WGPUShaderStage_Compute;
    input_tile_ids_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_tile_ids_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_hashmap_key_buffer_entry {};
    input_hashmap_key_buffer_entry.binding = 1;
    input_hashmap_key_buffer_entry.visibility = WGPUShaderStage_Compute;
    input_hashmap_key_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_hashmap_key_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_hashmap_value_buffer_entry {};
    input_hashmap_value_buffer_entry.binding = 2;
    input_hashmap_value_buffer_entry.visibility = WGPUShaderStage_Compute;
    input_hashmap_value_buffer_entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_hashmap_value_buffer_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_height_textures_entry {};
    input_height_textures_entry.binding = 3;
    input_height_textures_entry.visibility = WGPUShaderStage_Compute;
    input_height_textures_entry.texture.sampleType = WGPUTextureSampleType_Uint;
    input_height_textures_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;

    WGPUBindGroupLayoutEntry input_height_textures_sampler_entry {};
    input_height_textures_sampler_entry.binding = 4;
    input_height_textures_sampler_entry.visibility = WGPUShaderStage_Compute;
    input_height_textures_sampler_entry.sampler.type = WGPUSamplerBindingType_NonFiltering;

    WGPUBindGroupLayoutEntry output_tiles_entry {};
    output_tiles_entry.binding = 5;
    output_tiles_entry.visibility = WGPUShaderStage_Compute;
    output_tiles_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2DArray;
    output_tiles_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_tiles_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_d8_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> { input_tile_ids_entry, input_hashmap_key_buffer_entry, input_hashmap_value_buffer_entry,
            input_height_textures_entry, input_height_textures_sampler_entry, output_tiles_entry },
        "d8 compute bind group layout");
}

void PipelineManager::create_release_points_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_settings_entry {};
    input_settings_entry.binding = 0;
    input_settings_entry.visibility = WGPUShaderStage_Compute;
    input_settings_entry.buffer.type = WGPUBufferBindingType_Uniform;
    input_settings_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_normal_textures_entry {};
    input_normal_textures_entry.binding = 1;
    input_normal_textures_entry.visibility = WGPUShaderStage_Compute;
    input_normal_textures_entry.texture.sampleType = WGPUTextureSampleType_Float;
    input_normal_textures_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry output_release_points_texture_entry {};
    output_release_points_texture_entry.binding = 2;
    output_release_points_texture_entry.visibility = WGPUShaderStage_Compute;
    output_release_points_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    output_release_points_texture_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_release_points_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_release_point_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            input_settings_entry,
            input_normal_textures_entry,
            output_release_points_texture_entry,
        },
        "release point compute bind group layout");
}

void PipelineManager::create_height_decode_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_settings_entry {};
    input_settings_entry.binding = 0;
    input_settings_entry.visibility = WGPUShaderStage_Compute;
    input_settings_entry.buffer.type = WGPUBufferBindingType_Uniform;
    input_settings_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_texture_entry {};
    input_texture_entry.binding = 1;
    input_texture_entry.visibility = WGPUShaderStage_Compute;
    input_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    input_texture_entry.storageTexture.access = WGPUStorageTextureAccess_ReadOnly;
    input_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Uint;

    WGPUBindGroupLayoutEntry output_texture_entry {};
    output_texture_entry.binding = 2;
    output_texture_entry.visibility = WGPUShaderStage_Compute;
    output_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    output_texture_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_texture_entry.storageTexture.format = WGPUTextureFormat_R32Float;

    m_height_decode_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            input_settings_entry,
            input_texture_entry,
            output_texture_entry,
        },
        "height decode compute bind group layout");
}


void PipelineManager::create_mipmap_creation_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_texture_entry {};
    input_texture_entry.binding = 0;
    input_texture_entry.visibility = WGPUShaderStage_Compute;
    input_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    input_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;
    // input_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    // input_texture_entry.storageTexture.access = WGPUStorageTextureAccess_ReadOnly;
    // input_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    WGPUBindGroupLayoutEntry output_texture_entry {};
    output_texture_entry.binding = 1;

    output_texture_entry.visibility = WGPUShaderStage_Compute;
    output_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    output_texture_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_mipmap_creation_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(
        m_device, std::vector<WGPUBindGroupLayoutEntry> { input_texture_entry, output_texture_entry }, "mipmap creation bind group layout");
}

void PipelineManager::create_fxaa_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_texture_entry {};
    input_texture_entry.binding = 0;
    input_texture_entry.visibility = WGPUShaderStage_Compute;
    input_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    input_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry input_sampler_entry {};
    input_sampler_entry.binding = 1;
    input_sampler_entry.visibility = WGPUShaderStage_Compute;
    input_sampler_entry.sampler.type = WGPUSamplerBindingType_Filtering;
    input_sampler_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry output_texture_entry {};
    output_texture_entry.binding = 2;
    output_texture_entry.visibility = WGPUShaderStage_Compute;
    output_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    output_texture_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_fxaa_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            input_texture_entry,
            input_sampler_entry,
            output_texture_entry,
        },
        "fxaa bind group layout");
}

void PipelineManager::create_iterative_simulation_compute_bind_group_layout()
{
    WGPUBindGroupLayoutEntry input_settings_entry {};
    input_settings_entry.binding = 0;
    input_settings_entry.visibility = WGPUShaderStage_Compute;
    input_settings_entry.buffer.type = WGPUBufferBindingType_Uniform;
    input_settings_entry.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry input_height_textures_entry {};
    input_height_textures_entry.binding = 1;
    input_height_textures_entry.visibility = WGPUShaderStage_Compute;
    input_height_textures_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    input_height_textures_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry input_release_points_texture_entry {};
    input_release_points_texture_entry.binding = 2;
    input_release_points_texture_entry.visibility = WGPUShaderStage_Compute;
    input_release_points_texture_entry.texture.sampleType = WGPUTextureSampleType_Float;
    input_release_points_texture_entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutEntry input_parents {};
    input_parents.binding = 3;
    input_parents.visibility = WGPUShaderStage_Compute;
    input_parents.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    input_parents.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry flux {};
    flux.binding = 4;
    flux.visibility = WGPUShaderStage_Compute;
    flux.buffer.type = WGPUBufferBindingType_Storage;
    flux.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_parents {};
    output_parents.binding = 5;
    output_parents.visibility = WGPUShaderStage_Compute;
    output_parents.buffer.type = WGPUBufferBindingType_Storage;
    output_parents.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry output_texture_entry {};
    output_texture_entry.binding = 6;
    output_texture_entry.visibility = WGPUShaderStage_Compute;
    output_texture_entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    output_texture_entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    output_texture_entry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;

    m_iterative_simulation_compute_bind_group_layout = std::make_unique<webgpu::raii::BindGroupLayout>(m_device,
        std::vector<WGPUBindGroupLayoutEntry> {
            input_settings_entry,
            input_height_textures_entry,
            input_release_points_texture_entry,
            input_parents,
            flux,
            output_parents,
            output_texture_entry,
        },
        "iterative simulation bind group layout");
}

}

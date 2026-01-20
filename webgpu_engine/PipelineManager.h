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

#include "ShaderModuleManager.h"

#include <webgpu/raii/BindGroupLayout.h>
#include <webgpu/raii/CombinedComputePipeline.h>
#include <webgpu/raii/Pipeline.h>
#include <webgpu/webgpu.h>

namespace webgpu_engine {

class PipelineManager {
public:
    PipelineManager(WGPUDevice device, const ShaderModuleManager& shader_manager);

    const webgpu::raii::GenericRenderPipeline& render_tiles_pipeline() const;
    const webgpu::raii::GenericRenderPipeline& render_atmosphere_pipeline() const;
    const webgpu::raii::RenderPipeline& render_lines_pipeline() const;
    const webgpu::raii::GenericRenderPipeline& compose_pipeline() const;

    const webgpu::raii::CombinedComputePipeline& normals_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& snow_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& downsample_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& upsample_textures_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& avalanche_trajectories_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& buffer_to_texture_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& avalanche_influence_area_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& d8_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& release_point_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& height_decode_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& fxaa_compute_pipeline() const;
    const webgpu::raii::CombinedComputePipeline& iterative_simulation_compute_pipeline() const;

    const webgpu::raii::BindGroupLayout& shared_config_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& camera_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& tile_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& compose_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& normals_compute_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& snow_compute_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& downsample_compute_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& upsample_textures_compute_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& lines_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& depth_texture_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& avalanche_trajectories_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& buffer_to_texture_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& avalanche_influence_area_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& d8_compute_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& release_point_compute_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& height_decode_compute_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& fxaa_compute_bind_group_layout() const;
    const webgpu::raii::BindGroupLayout& iterative_simulation_compute_bind_group_layout() const;

    const webgpu::raii::CombinedComputePipeline& mipmap_creation_pipeline() const;
    const webgpu::raii::BindGroupLayout& mipmap_creation_bind_group_layout() const;

    void create_pipelines();
    void create_bind_group_layouts();
    void release_pipelines();
    bool pipelines_created() const;

private:
    void create_render_tiles_pipeline();
    void create_render_atmosphere_pipeline();
    void create_render_lines_pipeline();
    void create_compose_pipeline();
    void create_shadow_pipeline();
    void create_normals_compute_pipeline();
    void create_snow_compute_pipeline();
    void create_downsample_compute_pipeline();
    void create_upsample_textures_compute_pipeline();
    void create_avalanche_trajectories_compute_pipeline();
    void create_buffer_to_texture_compute_pipeline();
    void create_avalanche_influence_area_compute_pipeline();
    void create_d8_compute_pipeline();
    void create_release_point_compute_pipeline();
    void create_height_decode_compute_pipeline();
    void create_fxaa_compute_pipeline();
    void create_iterative_simulation_compute_pipeline();

    void create_shared_config_bind_group_layout();
    void create_camera_bind_group_layout();
    void create_tile_bind_group_layout();
    void create_compose_bind_group_layout();
    void create_normals_compute_bind_group_layout();
    void create_snow_compute_bind_group_layout();
    void create_downsample_compute_bind_group_layout();
    void create_upsample_textures_compute_bind_group_layout();
    void create_lines_bind_group_layout();
    void create_depth_texture_bind_group_layout();
    void create_avalanche_trajectory_bind_group_layout();
    void create_buffer_to_texture_bind_group_layout();
    void create_avalanche_influence_area_bind_group_layout();
    void create_d8_compute_bind_group_layout();
    void create_release_points_compute_bind_group_layout();
    void create_height_decode_compute_bind_group_layout();
    void create_fxaa_compute_bind_group_layout();
    void create_iterative_simulation_compute_bind_group_layout();

    void create_mipmap_creation_bind_group_layout();
    void create_mipmap_creation_pipeline();

private:
    WGPUDevice m_device;
    const ShaderModuleManager* m_shader_manager;

    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_render_tiles_pipeline;
    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_render_atmosphere_pipeline;
    std::unique_ptr<webgpu::raii::RenderPipeline> m_render_lines_pipeline;
    std::unique_ptr<webgpu::raii::GenericRenderPipeline> m_compose_pipeline;

    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_normals_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_snow_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_downsample_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_upsample_textures_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_avalanche_trajectories_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_avalanche_trajectories_buffer_to_texture_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_avalanche_influence_area_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_d8_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_release_point_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_height_decode_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_fxaa_compute_pipeline;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_iterative_simulation_compute_pipeline;

    std::unique_ptr<webgpu::raii::BindGroupLayout> m_shared_config_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_camera_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_tile_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_compose_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_normals_compute_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_downsample_compute_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_snow_compute_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_upsample_textures_compute_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_lines_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_depth_texture_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_avalanche_trajectories_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_avalanche_trajectories_buffer_to_texture_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_avalanche_influence_area_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_d8_compute_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_release_point_compute_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_height_decode_compute_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_fxaa_compute_bind_group_layout;
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_iterative_simulation_compute_bind_group_layout;

    // For MipMap-Creation
    std::unique_ptr<webgpu::raii::BindGroupLayout> m_mipmap_creation_bind_group_layout;
    std::unique_ptr<webgpu::raii::CombinedComputePipeline> m_mipmap_creation_compute_pipeline;

    bool m_pipelines_created = false;
};
}

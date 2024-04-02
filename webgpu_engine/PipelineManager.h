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
#include "raii/BindGroupWithLayout.h"
#include <webgpu/webgpu.h>

namespace webgpu_engine {

class PipelineManager {
public:
    PipelineManager(WGPUDevice device, ShaderModuleManager& shader_manager);

    const raii::RenderPipeline& debug_triangle_pipeline() const;
    const raii::RenderPipeline& debug_config_and_camera_pipeline() const;
    const raii::RenderPipeline& tile_pipeline() const;

    void create_pipelines(WGPUTextureFormat color_target_format, WGPUTextureFormat depth_texture_format, const raii::BindGroupWithLayout& bind_group_info,
        const raii::BindGroupWithLayout& shared_config_bind_group, const raii::BindGroupWithLayout& camera_bind_group,
        const raii::BindGroupWithLayout& tile_bind_group);
    void release_pipelines();
    bool pipelines_created() const;

private:
    void create_debug_pipeline(WGPUTextureFormat color_target_format);
    void create_debug_config_and_camera_pipeline(
        WGPUTextureFormat color_target_format, WGPUTextureFormat depth_texture_format, const raii::BindGroupWithLayout& bind_group_info);
    void create_tile_pipeline(WGPUTextureFormat color_target_format, WGPUTextureFormat depth_texture_format,
        const raii::BindGroupWithLayout& shared_config_bind_group, const raii::BindGroupWithLayout& camera_bind_group,
        const raii::BindGroupWithLayout& tile_bind_group);
    void create_shadow_pipeline();

private:
    WGPUDevice m_device;
    ShaderModuleManager* m_shader_manager;

    std::unique_ptr<raii::RenderPipeline> m_debug_triangle_pipeline;
    std::unique_ptr<raii::RenderPipeline> m_debug_config_and_camera_pipeline;
    std::unique_ptr<raii::RenderPipeline> m_tile_pipeline;

    bool m_pipelines_created = false;
};

}

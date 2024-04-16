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
#include "raii/BindGroupLayout.h"
#include "raii/Pipeline.h"
#include <webgpu/webgpu.h>

namespace webgpu_engine {

class PipelineManager {
public:
    PipelineManager(WGPUDevice device, ShaderModuleManager& shader_manager);

    const raii::RenderPipeline& tile_pipeline() const;
    const raii::RenderPipeline& compose_pipeline() const;

    const raii::BindGroupLayout& shared_config_bind_group_layout() const;
    const raii::BindGroupLayout& camera_bind_group_layout() const;
    const raii::BindGroupLayout& tile_bind_group_layout() const;
    const raii::BindGroupLayout& compose_bind_group_layout() const;

    void create_pipelines();
    void create_bind_group_layouts();
    void release_pipelines();
    bool pipelines_created() const;

private:
    void create_tile_pipeline();
    void create_compose_pipeline();
    void create_shadow_pipeline();

private:
    WGPUDevice m_device;
    ShaderModuleManager* m_shader_manager;

    std::unique_ptr<raii::GenericRenderPipeline> m_tile_pipeline;
    std::unique_ptr<raii::GenericRenderPipeline> m_compose_pipeline;

    std::unique_ptr<raii::BindGroupLayout> m_shared_config_bind_group_layout;
    std::unique_ptr<raii::BindGroupLayout> m_camera_bind_group_layout;
    std::unique_ptr<raii::BindGroupLayout> m_tile_bind_group_layout;
    std::unique_ptr<raii::BindGroupLayout> m_compose_bind_group_layout;

    bool m_pipelines_created = false;
};
}

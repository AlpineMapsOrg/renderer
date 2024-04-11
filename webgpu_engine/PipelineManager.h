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
#include "raii/Pipeline.h"
#include <webgpu/webgpu.h>

namespace webgpu_engine {

class PipelineManager {
public:
    PipelineManager(WGPUDevice device, ShaderModuleManager& shader_manager);

    const raii::RenderPipeline& tile_pipeline() const;
    const raii::RenderPipeline& compose_pipeline() const;

    void create_pipelines(const FramebufferFormat& framebuffer_format, const raii::BindGroupWithLayout& shared_config_bind_group,
        const raii::BindGroupWithLayout& camera_bind_group, const raii::BindGroupWithLayout& tile_bind_group,
        const raii::BindGroupWithLayout& compose_bind_group);
    void release_pipelines();
    bool pipelines_created() const;

private:
    void create_tile_pipeline(const FramebufferFormat& framebuffer_format, const raii::BindGroupWithLayout& shared_config_bind_group,
        const raii::BindGroupWithLayout& camera_bind_group, const raii::BindGroupWithLayout& tile_bind_group);
    void create_compose_pipeline(const FramebufferFormat& framebuffer_format, const raii::BindGroupWithLayout& shared_config_bind_group,
        const raii::BindGroupWithLayout& compose_bind_group);
    void create_shadow_pipeline();

private:
    WGPUDevice m_device;
    ShaderModuleManager* m_shader_manager;

    std::unique_ptr<raii::GenericRenderPipeline> m_tile_pipeline;
    std::unique_ptr<raii::GenericRenderPipeline> m_compose_pipeline;

    bool m_pipelines_created = false;
};

}

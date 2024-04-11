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

#include "raii/Pipeline.h"
#include "util/VertexBufferInfo.h"

namespace webgpu_engine {

PipelineManager::PipelineManager(WGPUDevice device, ShaderModuleManager& shader_manager)
    : m_device(device)
    , m_shader_manager(&shader_manager)
{
}

const raii::RenderPipeline& PipelineManager::tile_pipeline() const { return m_tile_pipeline->pipeline(); }

const raii::RenderPipeline& PipelineManager::compose_pipeline() const { return m_compose_pipeline->pipeline(); }

void PipelineManager::create_pipelines(const FramebufferFormat& framebuffer_format, const raii::BindGroupWithLayout& shared_config_bind_group,
    const raii::BindGroupWithLayout& camera_bind_group, const raii::BindGroupWithLayout& tile_bind_group, const raii::BindGroupWithLayout& compose_bind_group)
{
    create_tile_pipeline(framebuffer_format, shared_config_bind_group, camera_bind_group, tile_bind_group);
    create_compose_pipeline(framebuffer_format, shared_config_bind_group, compose_bind_group);
    m_pipelines_created = true;
}

void PipelineManager::release_pipelines()
{
    m_tile_pipeline.release();
    m_compose_pipeline.release();
    m_pipelines_created = false;
}

bool PipelineManager::pipelines_created() const { return m_pipelines_created; }

void PipelineManager::create_tile_pipeline(const FramebufferFormat& framebuffer_format, const raii::BindGroupWithLayout& shared_config_bind_group,
    const raii::BindGroupWithLayout& camera_bind_group, const raii::BindGroupWithLayout& tile_bind_group)
{
    util::SingleVertexBufferInfo bounds_buffer_info(WGPUVertexStepMode_Instance);
    bounds_buffer_info.add_attribute<float, 4>(0);
    util::SingleVertexBufferInfo texture_layer_buffer_info(WGPUVertexStepMode_Instance);
    texture_layer_buffer_info.add_attribute<int32_t, 1>(1);
    util::SingleVertexBufferInfo tileset_id_buffer_info(WGPUVertexStepMode_Instance);
    tileset_id_buffer_info.add_attribute<int32_t, 1>(2);
    util::SingleVertexBufferInfo zoomlevel_buffer_info(WGPUVertexStepMode_Instance);
    zoomlevel_buffer_info.add_attribute<int32_t, 1>(3);

    std::vector<const raii::BindGroupLayout*> bind_group_layouts
        = { &shared_config_bind_group.bind_group_layout(), &camera_bind_group.bind_group_layout(), &tile_bind_group.bind_group_layout() };

    m_tile_pipeline = std::make_unique<raii::GenericRenderPipeline>(m_device, m_shader_manager->tile(), m_shader_manager->tile(),
        std::vector<util::SingleVertexBufferInfo> { bounds_buffer_info, texture_layer_buffer_info, tileset_id_buffer_info, zoomlevel_buffer_info },
        framebuffer_format,
        std::vector<const raii::BindGroupLayout*> {
            &shared_config_bind_group.bind_group_layout(), &camera_bind_group.bind_group_layout(), &tile_bind_group.bind_group_layout() });
}

void PipelineManager::create_compose_pipeline(
    const FramebufferFormat& framebuffer_format, const raii::BindGroupWithLayout& shared_config_bind_group, const raii::BindGroupWithLayout& compose_bind_group)
{
    m_compose_pipeline = std::make_unique<raii::GenericRenderPipeline>(m_device, m_shader_manager->screen_pass_vert(), m_shader_manager->compose_frag(),
        std::vector<util::SingleVertexBufferInfo> {}, framebuffer_format,
        std::vector<const raii::BindGroupLayout*> { &shared_config_bind_group.bind_group_layout(), &compose_bind_group.bind_group_layout() });
}

void PipelineManager::create_shadow_pipeline() {
    //TODO
}
}

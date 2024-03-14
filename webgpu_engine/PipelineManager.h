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

#include "webgpu.hpp"
#include "ShaderModuleManager.h"
#include "UniformBuffer.h"

namespace webgpu_engine {

class BindGroupInfo {
public:
    BindGroupInfo() = default;

    template<class T>
    void add_entry(uint32_t binding, const UniformBuffer<T>& buf, wgpu::ShaderStageFlags visibility) {
        wgpu::BufferBindingLayout buffer_binding_layout;
        buffer_binding_layout.type = wgpu::BufferBindingType::Uniform;
        buffer_binding_layout.minBindingSize = sizeof(T);
        buffer_binding_layout.nextInChain = nullptr;
        buffer_binding_layout.hasDynamicOffset = false;

        wgpu::BindGroupLayoutEntry layout_entry{};
        layout_entry.binding = binding;
        layout_entry.visibility = visibility;
        layout_entry.buffer = buffer_binding_layout;
        m_bind_group_layout_entries.push_back(layout_entry);

        wgpu::BindGroupEntry entry{};
        entry.binding = binding;
        entry.buffer = buf.handle();
        entry.size = sizeof(T);
        entry.offset = 0;
        entry.nextInChain = nullptr;
        m_bind_group_entries.push_back(entry);
    }

    void create_bind_group_layout(wgpu::Device& device);
    void create_bind_group(wgpu::Device& device);
    void init(wgpu::Device& device);

    void bind(wgpu::RenderPassEncoder& render_pass, uint32_t group_index);

private:
    std::vector<wgpu::BindGroupEntry> m_bind_group_entries;
    std::vector<wgpu::BindGroupLayoutEntry> m_bind_group_layout_entries;

public:
    wgpu::BindGroupLayout m_bind_group_layout = nullptr;
    wgpu::BindGroup m_bind_group = nullptr;
};

class PipelineManager {
public:
    PipelineManager(wgpu::Device& device, ShaderModuleManager& shader_manager);

    wgpu::RenderPipeline debug_triangle_pipeline() const;
    wgpu::RenderPipeline debug_config_and_camera_pipeline() const;

    void create_pipelines(wgpu::TextureFormat color_target_format, BindGroupInfo& bindGroup);
    void release_pipelines();
    bool pipelines_created();

private:
    void create_debug_pipeline(wgpu::TextureFormat color_target_format);
    void create_debug_config_and_camera_pipeline(wgpu::TextureFormat color_target_format, BindGroupInfo& bind_group_info);
    void create_tile_pipeline();
    void create_shadow_pipeline();

private:
    wgpu::Device* m_device;
    ShaderModuleManager* m_shader_manager;

    wgpu::RenderPipeline m_debug_triangle_pipeline = nullptr;
    wgpu::RenderPipeline m_debug_config_and_camera_pipeline = nullptr;
    wgpu::RenderPipeline m_tile_pipeline = nullptr;
    wgpu::RenderPipeline m_shadow_pipeline = nullptr;
};

}

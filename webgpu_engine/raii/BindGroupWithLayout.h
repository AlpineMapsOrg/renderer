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

#include "Buffer.h"
#include "TextureView.h"
#include <string>
#include <vector>
#include <webgpu/webgpu.h>

namespace webgpu_engine::raii {

class BindGroupWithLayoutInfo;

class BindGroupWithLayout {
public:
    BindGroupWithLayout(WGPUDevice device, const BindGroupWithLayoutInfo& bind_group_with_layout);

    void bind(WGPURenderPassEncoder& render_pass, uint32_t group_index) const;

    const raii::BindGroupLayout& bind_group_layout() const;
    const raii::BindGroup& bind_group() const;

private:
    std::unique_ptr<raii::BindGroupLayout> m_bind_group_layout;
    std::unique_ptr<raii::BindGroup> m_bind_group;
};

class BindGroupWithLayoutInfo {
public:
    BindGroupWithLayoutInfo(const std::string& label = "name not set");

    template <class T> void add_entry(uint32_t binding, const raii::Buffer<T>& buf, WGPUShaderStageFlags visibility);
    void add_entry(uint32_t binding, const raii::TextureView& texture_view, WGPUShaderStageFlags visibility, WGPUTextureSampleType sample_type);
    void add_entry(uint32_t binding, const raii::Sampler& sampler, WGPUShaderStageFlags visibility, WGPUSamplerBindingType binding_type);

    void bind(WGPURenderPassEncoder& render_pass, uint32_t group_index) const;

private:
    WGPUBindGroupLayoutDescriptor bind_group_layout_descriptor() const;
    WGPUBindGroupDescriptor bind_group_descriptor(WGPUBindGroupLayout layout) const;

    friend BindGroupWithLayout::BindGroupWithLayout(WGPUDevice device, const BindGroupWithLayoutInfo& bind_group_with_layout);

private:
    std::vector<WGPUBindGroupEntry> m_bind_group_entries;
    std::vector<WGPUBindGroupLayoutEntry> m_bind_group_layout_entries;
    std::string m_label;
};

template <class T> void BindGroupWithLayoutInfo::add_entry(uint32_t binding, const raii::Buffer<T>& buf, WGPUShaderStageFlags visibility)
{
    WGPUBufferBindingLayout buffer_binding_layout;
    buffer_binding_layout.type = WGPUBufferBindingType::WGPUBufferBindingType_Uniform;
    buffer_binding_layout.minBindingSize = sizeof(T);
    buffer_binding_layout.nextInChain = nullptr;
    buffer_binding_layout.hasDynamicOffset = false;

    WGPUBindGroupLayoutEntry layout_entry {};
    layout_entry.binding = binding;
    layout_entry.visibility = visibility;
    layout_entry.buffer = buffer_binding_layout;
    m_bind_group_layout_entries.push_back(layout_entry);

    WGPUBindGroupEntry entry {};
    entry.binding = binding;
    entry.buffer = buf.handle();
    entry.size = sizeof(T);
    entry.offset = 0;
    entry.nextInChain = nullptr;
    m_bind_group_entries.push_back(entry);
}

} // namespace webgpu_engine::raii

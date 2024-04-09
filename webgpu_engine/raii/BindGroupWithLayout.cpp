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

#include "BindGroupWithLayout.h"

namespace webgpu_engine::raii {

BindGroupWithLayout::BindGroupWithLayout(WGPUDevice device, const BindGroupWithLayoutInfo& bind_group_with_layout)
    : m_bind_group_layout(std::make_unique<raii::BindGroupLayout>(device, bind_group_with_layout.bind_group_layout_descriptor()))
    , m_bind_group(std::make_unique<raii::BindGroup>(device, bind_group_with_layout.bind_group_descriptor(m_bind_group_layout->handle())))
{
}

void BindGroupWithLayout::bind(WGPURenderPassEncoder& render_pass, uint32_t group_index) const
{
    wgpuRenderPassEncoderSetBindGroup(render_pass, group_index, m_bind_group->handle(), 0, nullptr);
}

const BindGroupLayout& BindGroupWithLayout::bind_group_layout() const { return *m_bind_group_layout; }

const BindGroup& BindGroupWithLayout::bind_group() const { return *m_bind_group; }

BindGroupWithLayoutInfo::BindGroupWithLayoutInfo(const std::string& label)
    : m_label { label }
{
}

WGPUBindGroupLayoutDescriptor BindGroupWithLayoutInfo::bind_group_layout_descriptor() const
{
    WGPUBindGroupLayoutDescriptor desc {};
    desc.label = m_label.data();
    desc.entries = m_bind_group_layout_entries.data();
    desc.entryCount = m_bind_group_layout_entries.size();
    return desc;
}

WGPUBindGroupDescriptor BindGroupWithLayoutInfo::bind_group_descriptor(WGPUBindGroupLayout layout) const
{
    WGPUBindGroupDescriptor desc {};
    desc.label = m_label.data();
    desc.layout = layout;
    desc.entries = m_bind_group_entries.data();
    desc.entryCount = m_bind_group_entries.size();
    return desc;
}

void BindGroupWithLayoutInfo::add_entry(
    uint32_t binding, const raii::TextureView& texture_view, WGPUShaderStageFlags visibility, WGPUTextureSampleType sample_type)
{
    WGPUTextureBindingLayout texture_binding_layout {};
    texture_binding_layout.multisampled = false;
    texture_binding_layout.sampleType = sample_type;
    texture_binding_layout.viewDimension = texture_view.dimension();
    texture_binding_layout.nextInChain = nullptr;

    WGPUBindGroupLayoutEntry layout_entry {};
    layout_entry.binding = binding;
    layout_entry.visibility = visibility;
    layout_entry.texture = texture_binding_layout;
    m_bind_group_layout_entries.push_back(layout_entry);

    WGPUBindGroupEntry entry {};
    entry.binding = binding;
    entry.textureView = texture_view.handle();
    entry.offset = 0;
    entry.nextInChain = nullptr;
    m_bind_group_entries.push_back(entry);
}

void BindGroupWithLayoutInfo::add_entry(uint32_t binding, const raii::Sampler& sampler, WGPUShaderStageFlags visibility, WGPUSamplerBindingType binding_type)
{
    WGPUSamplerBindingLayout sampler_binding_layout {};
    sampler_binding_layout.type = binding_type;
    sampler_binding_layout.nextInChain = nullptr;

    WGPUBindGroupLayoutEntry layout_entry {};
    layout_entry.binding = binding;
    layout_entry.visibility = visibility;
    layout_entry.sampler = sampler_binding_layout;
    m_bind_group_layout_entries.push_back(layout_entry);

    WGPUBindGroupEntry entry {};
    entry.binding = binding;
    entry.sampler = sampler.handle();
    entry.offset = 0;
    entry.nextInChain = nullptr;
    m_bind_group_entries.push_back(entry);
}

} // namespace webgpu_engine::raii

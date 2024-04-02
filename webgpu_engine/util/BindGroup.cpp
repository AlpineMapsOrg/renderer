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

#include "BindGroup.h"

namespace webgpu_engine::util {

BindGroupInfo::BindGroupInfo(const std::string& label)
    : m_label { label }
{
}

void BindGroupInfo::create_bind_group_layout(WGPUDevice device)
{
    WGPUBindGroupLayoutDescriptor desc {};
    desc.label = m_label.data();
    desc.entries = m_bind_group_layout_entries.data();
    desc.entryCount = m_bind_group_layout_entries.size();
    desc.nextInChain = nullptr;
    m_bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &desc);
}

void BindGroupInfo::create_bind_group(WGPUDevice device)
{
    WGPUBindGroupDescriptor desc {};
    desc.label = m_label.data();
    desc.layout = m_bind_group_layout;
    desc.entries = m_bind_group_entries.data();
    desc.entryCount = m_bind_group_entries.size();
    desc.nextInChain = nullptr;
    m_bind_group = wgpuDeviceCreateBindGroup(device, &desc);
}

void BindGroupInfo::init(WGPUDevice device)
{
    create_bind_group_layout(device);
    create_bind_group(device);
}

void BindGroupInfo::bind(WGPURenderPassEncoder& render_pass, uint32_t group_index) const
{
    wgpuRenderPassEncoderSetBindGroup(render_pass, group_index, m_bind_group, 0, nullptr);
}

void BindGroupInfo::add_entry(uint32_t binding, const raii::TextureView& texture_view, WGPUShaderStageFlags visibility, WGPUTextureSampleType sample_type)
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

void BindGroupInfo::add_entry(uint32_t binding, const raii::Sampler& sampler, WGPUShaderStageFlags visibility, WGPUSamplerBindingType binding_type)
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

} // namespace webgpu_engine::util

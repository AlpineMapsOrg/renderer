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

namespace webgpu::raii {

BindGroup::BindGroup(WGPUDevice device, WGPUBindGroupLayout layout, const std::initializer_list<WGPUBindGroupEntry>& entries, const std::string& label)
    : GpuResource(device,
          WGPUBindGroupDescriptor {
              .nextInChain = nullptr,
              .label = WGPUStringView { .data = label.c_str(), .length = label.length() },
              .layout = layout,
              .entryCount = entries.size(),
              .entries = entries.begin(),
          })
{
}

BindGroup::BindGroup(WGPUDevice device, WGPUBindGroupLayout layout, const std::vector<WGPUBindGroupEntry>& entries, const std::string& label)
    : GpuResource(device,
          WGPUBindGroupDescriptor {
              .nextInChain = nullptr,
              .label = WGPUStringView { .data = label.c_str(), .length = label.length() },
              .layout = layout,
              .entryCount = entries.size(),
              .entries = entries.data(),
          })
{
}

BindGroup::BindGroup(WGPUDevice device, const BindGroupLayout& layout, const std::initializer_list<WGPUBindGroupEntry>& entries, const std::string& label)
    : GpuResource(device,
          WGPUBindGroupDescriptor {
              .nextInChain = nullptr,
              .label = WGPUStringView { .data = label.c_str(), .length = label.length() },
              .layout = layout.handle(),
              .entryCount = entries.size(),
              .entries = entries.begin(),
          })
{
}

BindGroup::BindGroup(WGPUDevice device, const BindGroupLayout& layout, const std::vector<WGPUBindGroupEntry>& entries, const std::string& label)
    : GpuResource(device,
          WGPUBindGroupDescriptor {
              .nextInChain = nullptr,
              .label = WGPUStringView { .data = label.c_str(), .length = label.length() },
              .layout = layout.handle(),
              .entryCount = entries.size(),
              .entries = entries.data(),
          })
{
}

} // namespace webgpu::raii

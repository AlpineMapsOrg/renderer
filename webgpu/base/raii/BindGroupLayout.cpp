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

#include "BindGroupLayout.h"

#include <string>
#include <webgpu/webgpu.h>

namespace webgpu::raii {

BindGroupLayout::BindGroupLayout(WGPUDevice device, const std::vector<WGPUBindGroupLayoutEntry>& entries, const std::string& label)
    : GpuResource(device,
          WGPUBindGroupLayoutDescriptor {
              .nextInChain = nullptr,
              .label = { .data = label.data(), .length = label.length() },
              .entryCount = entries.size(),
              .entries = entries.data(),
          })
{
}

} // namespace webgpu::raii

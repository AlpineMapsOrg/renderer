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

#include "PipelineLayout.h"

namespace webgpu::raii {

PipelineLayout::PipelineLayout(WGPUDevice device, const std::vector<WGPUBindGroupLayout>& layouts, const std::string& label)
    : GpuResource(device,
          WGPUPipelineLayoutDescriptor {
              .nextInChain = nullptr,
              .label = WGPUStringView { .data = label.data(), .length = label.length() },
              .bindGroupLayoutCount = layouts.size(),
              .bindGroupLayouts = layouts.data(),
          })
{
}

} // namespace webgpu::raii

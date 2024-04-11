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

#include "util/BindGroupWithLayoutInfo.h"

namespace webgpu_engine::raii {

BindGroupWithLayout::BindGroupWithLayout(WGPUDevice device, const util::BindGroupWithLayoutInfo& bind_group_with_layout)
    : m_bind_group_layout(std::make_unique<raii::BindGroupLayout>(device, bind_group_with_layout.bind_group_layout_descriptor()))
    , m_bind_group(std::make_unique<raii::BindGroup>(device, bind_group_with_layout.bind_group_descriptor(m_bind_group_layout->handle())))
{
}

void BindGroupWithLayout::bind(WGPURenderPassEncoder render_pass, uint32_t group_index) const
{
    wgpuRenderPassEncoderSetBindGroup(render_pass, group_index, m_bind_group->handle(), 0, nullptr);
}

const BindGroupLayout& BindGroupWithLayout::bind_group_layout() const { return *m_bind_group_layout; }

const BindGroup& BindGroupWithLayout::bind_group() const { return *m_bind_group; }

} // namespace webgpu_engine::raii

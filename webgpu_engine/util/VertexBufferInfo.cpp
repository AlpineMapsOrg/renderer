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

#include "util/VertexBufferInfo.h"

#include <algorithm>
#include <cassert>
#include <iterator>

namespace webgpu_engine::util {

WGPUVertexBufferLayout SingleVertexBufferInfo::vertex_buffer_layout() const
{
    assert(m_step_mode != WGPUVertexStepMode_Undefined);
    assert(m_vertex_attributes.size() != 0);
    assert(m_stride != 0);

    WGPUVertexBufferLayout vertex_buffer_layout {};
    vertex_buffer_layout.arrayStride = m_stride;
    vertex_buffer_layout.attributeCount = m_vertex_attributes.size();
    vertex_buffer_layout.attributes = m_vertex_attributes.data();
    vertex_buffer_layout.stepMode = m_step_mode;
    return vertex_buffer_layout;
}

SingleVertexBufferInfo::SingleVertexBufferInfo(WGPUVertexStepMode step_mode)
    : m_stride(0)
    , m_explicit_stride(false)
    , m_step_mode(step_mode)
{
}

SingleVertexBufferInfo::SingleVertexBufferInfo(WGPUVertexStepMode step_mode, uint32_t stride)
    : m_stride(stride)
    , m_explicit_stride(true)
    , m_step_mode(step_mode)
{
}

void VertexBufferInfo::add_buffer(WGPUVertexStepMode step_mode) { m_vertex_buffer_info.emplace_back(step_mode); }

void VertexBufferInfo::add_buffer(WGPUVertexStepMode step_mode, uint32_t stride) { m_vertex_buffer_info.emplace_back(step_mode, stride); }

SingleVertexBufferInfo& VertexBufferInfo::get_buffer_info(size_t buffer_index) { return m_vertex_buffer_info.at(buffer_index); }

std::vector<WGPUVertexBufferLayout> VertexBufferInfo::vertex_buffer_layouts() const
{
    std::vector<WGPUVertexBufferLayout> layouts;
    auto to_wgpu_object = [](const SingleVertexBufferInfo& info) { return info.vertex_buffer_layout(); };
    std::transform(m_vertex_buffer_info.begin(), m_vertex_buffer_info.end(), std::back_inserter(layouts), to_wgpu_object);
    return layouts;
}

} // namespace webgpu_engine::util

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

#include "VertexFormat.h"
#include <vector>
#include <webgpu/webgpu.h>

namespace webgpu_engine::util {

class SingleVertexBufferInfo {
public:
    SingleVertexBufferInfo(WGPUVertexStepMode step_mode);
    SingleVertexBufferInfo(WGPUVertexStepMode step_mode, uint32_t stride);

    /// Adds attribute to this vertex buffer.
    /// T is component type and N is number of components
    /// For example, use add_attribute<float, 4>(0) to add an attribute with 4 floats at shader location 0.
    template <typename T, int N> void add_attribute(uint32_t shader_location, uint32_t offset = 0);

    WGPUVertexBufferLayout vertex_buffer_layout() const;

private:
    std::vector<WGPUVertexAttribute> m_vertex_attributes;
    size_t m_stride = 0;
    bool m_explicit_stride = false;
    WGPUVertexStepMode m_step_mode = WGPUVertexStepMode_Undefined;
};

class VertexBufferInfo {
public:
    VertexBufferInfo() = default;

    void add_buffer(WGPUVertexStepMode step_mode);
    void add_buffer(WGPUVertexStepMode step_mode, uint32_t stride);

    SingleVertexBufferInfo& get_buffer_info(size_t buffer_index);

    std::vector<WGPUVertexBufferLayout> vertex_buffer_layouts() const;

private:
    std::vector<SingleVertexBufferInfo> m_vertex_buffer_info;
};

template <typename T, int N> void SingleVertexBufferInfo::add_attribute(uint32_t shader_location, uint32_t offset)
{
    static_assert(VertexFormat<T, N>::format() != WGPUVertexFormat_Undefined);

    WGPUVertexAttribute attribute {};
    attribute.shaderLocation = shader_location;
    attribute.format = VertexFormat<T, N>::format();
    attribute.offset = offset;

    m_vertex_attributes.push_back(attribute);

    if (!m_explicit_stride) {
        m_stride += VertexFormat<T, N>::size();
    }
}

} // namespace webgpu_engine::util

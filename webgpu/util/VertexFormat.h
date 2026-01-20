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

#include <QDebug>
#include <webgpu/webgpu.h>

namespace webgpu::util {

template <typename T, int N> struct VertexFormat {
    static constexpr WGPUVertexFormat format();
    static constexpr size_t size() { return sizeof(T) * N; }
};

template <typename T, int N> constexpr WGPUVertexFormat VertexFormat<T, N>::format()
{
    static_assert(sizeof N != sizeof N, "tried to get unmapped vertex format");
    return static_cast<WGPUVertexFormat>(0);
}
template <> constexpr WGPUVertexFormat VertexFormat<float, 1>::format() { return WGPUVertexFormat_Float32; }
template <> constexpr WGPUVertexFormat VertexFormat<float, 2>::format() { return WGPUVertexFormat_Float32x2; }
template <> constexpr WGPUVertexFormat VertexFormat<float, 3>::format() { return WGPUVertexFormat_Float32x3; }
template <> constexpr WGPUVertexFormat VertexFormat<float, 4>::format() { return WGPUVertexFormat_Float32x4; }

template <> constexpr WGPUVertexFormat VertexFormat<uint32_t, 1>::format() { return WGPUVertexFormat_Uint32; }
template <> constexpr WGPUVertexFormat VertexFormat<uint32_t, 2>::format() { return WGPUVertexFormat_Uint32x2; }
template <> constexpr WGPUVertexFormat VertexFormat<uint32_t, 3>::format() { return WGPUVertexFormat_Uint32x3; }
template <> constexpr WGPUVertexFormat VertexFormat<uint32_t, 4>::format() { return WGPUVertexFormat_Uint32x4; }
template <> constexpr WGPUVertexFormat VertexFormat<uint16_t, 2>::format() { return WGPUVertexFormat_Uint16x2; }
template <> constexpr WGPUVertexFormat VertexFormat<uint16_t, 4>::format() { return WGPUVertexFormat_Uint16x4; }
template <> constexpr WGPUVertexFormat VertexFormat<uint8_t, 2>::format() { return WGPUVertexFormat_Uint8x2; }
template <> constexpr WGPUVertexFormat VertexFormat<uint8_t, 4>::format() { return WGPUVertexFormat_Uint8x4; }

template <> constexpr WGPUVertexFormat VertexFormat<int32_t, 1>::format() { return WGPUVertexFormat_Sint32; }
template <> constexpr WGPUVertexFormat VertexFormat<int32_t, 2>::format() { return WGPUVertexFormat_Sint32x2; }
template <> constexpr WGPUVertexFormat VertexFormat<int32_t, 3>::format() { return WGPUVertexFormat_Sint32x3; }
template <> constexpr WGPUVertexFormat VertexFormat<int32_t, 4>::format() { return WGPUVertexFormat_Sint32x4; }
template <> constexpr WGPUVertexFormat VertexFormat<int16_t, 2>::format() { return WGPUVertexFormat_Sint16x2; }
template <> constexpr WGPUVertexFormat VertexFormat<int16_t, 4>::format() { return WGPUVertexFormat_Sint16x4; }
template <> constexpr WGPUVertexFormat VertexFormat<int8_t, 2>::format() { return WGPUVertexFormat_Sint8x2; }
template <> constexpr WGPUVertexFormat VertexFormat<int8_t, 4>::format() { return WGPUVertexFormat_Sint8x4; }

} // namespace webgpu::util

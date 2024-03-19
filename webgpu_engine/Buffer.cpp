/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Gerald Kimmersdorfer
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
#include "Buffer.h"
#include "UniformBufferObjects.h"

namespace webgpu_engine {

template <typename T>
Buffer<T>::Buffer(wgpu::Device device, wgpu::BufferUsageFlags flags)
    : m_non_backed_buffer(device, flags, 1)
{
}

template <typename T> void Buffer<T>::update_gpu_data(wgpu::Queue queue) { m_non_backed_buffer.write(queue, &data, 1, 0); }

template <typename T> QString Buffer<T>::data_as_string() { return ubo_as_string(data); }

template <typename T> bool Buffer<T>::data_from_string(const QString& base64String)
{
    bool result = true;
    auto newData = ubo_from_string<T>(base64String, &result);
    if (result) data = newData;
    return result;
}

template <typename T> wgpu::Buffer Buffer<T>::handle() const { return m_non_backed_buffer.handle(); }

// IMPORTANT: All possible Template Classes need to be defined here:
template class Buffer<uboSharedConfig>;
template class Buffer<uboCameraConfig>;
//TODO
//template class UniformBuffer<uboShadowConfig>;
template class Buffer<uboTestConfig>;
}

/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include <QString>
#include <webgpu/raii/RawBuffer.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_interface.hpp>

namespace webgpu_engine {

/// Generic class for buffers that are backed by a member variable.
template <typename T> class Buffer {
public:
    // Creates a Buffer object representing a region in GPU memory.
    Buffer(WGPUDevice device, WGPUBufferUsage flags);

    // Refills the GPU Buffer
    void update_gpu_data(WGPUQueue queue);

    // Returns String representation of buffer data (Base64)
    QString data_as_string();

    // Loads the given base 64 encoded string as the buffer data
    bool data_from_string(const QString& base64String);

    const webgpu::raii::RawBuffer<T>& raw_buffer() const;

public:
    // Contains the buffer data
    T data;

protected:
    webgpu::raii::RawBuffer<T> m_raw_buffer;
};

} // namespace webgpu_engine

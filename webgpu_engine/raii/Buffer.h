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
#include <webgpu/webgpu.h>

namespace webgpu_engine::raii {

/// Generic class for GPU buffer handles complying to RAII.
/// This class does not store the value to be written on CPU side.
template <typename T> class RawBuffer {
public:
    // m_size in num objects
    RawBuffer(WGPUDevice device, WGPUBufferUsageFlags usage, size_t size)
        : m_size(size)
    {
        WGPUBufferDescriptor bufferDesc {};
        bufferDesc.usage = usage;
        bufferDesc.size = size * sizeof(T); // takes size in bytes
        bufferDesc.mappedAtCreation = false;
        m_buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
        // this->update_gpu_data();
    }

    // delete copy constructor and copy-assignment operator
    RawBuffer(const RawBuffer& other) = delete;
    RawBuffer& operator=(const RawBuffer& other) = delete;

    ~RawBuffer()
    {
        wgpuBufferDestroy(m_buffer);
        wgpuBufferRelease(m_buffer);
    }

    void write(WGPUQueue queue, const T* data, size_t count = 1, size_t offset = 0)
    {
        assert(count <= m_size);
        wgpuQueueWriteBuffer(queue, m_buffer, offset, data, count * sizeof(T)); // takes size in bytes
    }

    WGPUBuffer handle() const { return m_buffer; }
    size_t size() const { return m_size; }
    size_t size_in_byte() const { return m_size * sizeof(T); };

private:
    WGPUBuffer m_buffer = nullptr;
    size_t m_size;
};

/// Generic class for buffers that are backed by a member variable.
template <typename T> class Buffer {
public:
    // Creates a Buffer object representing a region in GPU memory.
    Buffer(WGPUDevice device, WGPUBufferUsageFlags flags);

    // Refills the GPU Buffer
    void update_gpu_data(WGPUQueue queue);

    // Returns String representation of buffer data (Base64)
    QString data_as_string();

    // Loads the given base 64 encoded string as the buffer data
    bool data_from_string(const QString& base64String);

    // Retrieves underlying buffer handle
    WGPUBuffer handle() const;

public:
    // Contains the buffer data
    T data;

protected:
    RawBuffer<T> m_raw_buffer;
};

} // namespace webgpu_engine::raii

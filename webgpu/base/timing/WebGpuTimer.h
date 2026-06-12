/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "../raii/RawBuffer.h"
#include "TimerInterface.h"
#include <memory>
#include <vector>
#include <webgpu/webgpu.h>

namespace webgpu::timing {

class WebGpuTimer : public TimerInterface {

public:
    WebGpuTimer(WGPUDevice device, uint32_t ring_buffer_size, size_t capacity);
    void start(WGPUCommandEncoder encoder);
    void stop(WGPUCommandEncoder encoder);
    /// Readback of last value. Needs to be called after queue submit!
    void resolve();

private:
    WGPUQuerySetDescriptor m_timestamp_query_desc;
    WGPUQuerySet m_timestamp_queries;
    WGPUPassTimestampWrites m_timestamp_writes;
    WGPUDevice m_device;

    std::unique_ptr<webgpu::raii::RawBuffer<uint64_t>> m_timestamp_resolve;
    std::vector<std::unique_ptr<webgpu::raii::RawBuffer<uint64_t>>> m_timestamp_readback_buffer;

    int m_ringbuffer_index_write = 0; // next write index
    int m_ringbuffer_index_read = -1; // next index to read
    inline void increment_index(int& index) { index = (index + 1) % this->m_timestamp_readback_buffer.size(); }

#ifdef QT_DEBUG
    uint32_t m_dbg_dropped_measurement_count = 0;
#endif
};

} // namespace webgpu::timing

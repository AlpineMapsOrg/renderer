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

#include "WebGpuTimer.h"

#include <QDebug>

namespace webgpu::timing {

#ifdef QT_DEBUG
const char* readback_timer_names(uint32_t id)
{
    if (id == 0)
        return "Timestamp Readback 1";
    else if (id == 1)
        return "Timestamp Readback 2";
    else if (id == 2)
        return "Timestamp Readback 3";
    else if (id == 3)
        return "Timestamp Readback 4";
    else
        return "Timestamp Readback X";
}
#else
inline const char* readback_timer_names([[maybe_unused]] uint32_t id) { return "Timestamp Readback"; }
#endif

WebGpuTimer::WebGpuTimer(WGPUDevice device, uint32_t ring_buffer_size, size_t capacity)
    : TimerInterface(capacity)
    , m_device(device)
{
    m_timestamp_query_desc = {
        .nextInChain = nullptr,
        .label = WGPUStringView { .data = "Timing Query", .length = WGPU_STRLEN },
        .type = WGPUQueryType_Timestamp,
        .count = 2, // start and end
    };
    m_timestamp_queries = wgpuDeviceCreateQuerySet(m_device, &m_timestamp_query_desc);
    m_timestamp_writes = {
        .nextInChain = nullptr,
        .querySet = m_timestamp_queries,
        .beginningOfPassWriteIndex = 0,
        .endOfPassWriteIndex = 1,
    };
    m_timestamp_resolve
        = std::make_unique<webgpu::raii::RawBuffer<uint64_t>>(device, WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc, 2, "Timestamp GPU Buffer");

    for (uint32_t i = 0; i < ring_buffer_size; ++i) {
        m_timestamp_readback_buffer.push_back(
            std::make_unique<webgpu::raii::RawBuffer<uint64_t>>(device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, 2, readback_timer_names(i)));
    }
}

void WebGpuTimer::start(WGPUCommandEncoder encoder)
{
    // Query the GPU for the start timestamp
    wgpuCommandEncoderWriteTimestamp(encoder, m_timestamp_queries, 0);
}

void WebGpuTimer::stop(WGPUCommandEncoder encoder)
{
    const uint32_t size_2_uint64 = static_cast<uint32_t>(m_timestamp_resolve->size_in_byte());
    // Query the GPU for the stop timestamp
    wgpuCommandEncoderWriteTimestamp(encoder, m_timestamp_queries, 1);
    // Resolve the query set into the resolve buffer
    wgpuCommandEncoderResolveQuerySet(encoder, m_timestamp_queries, 0, 2, m_timestamp_resolve->handle(), 0);
    // Copy the resolve buffer to the result buffer
    const auto i = m_ringbuffer_index_write;
    if (m_timestamp_readback_buffer[i]->map_state() == WGPUBufferMapState_Unmapped) {
        m_timestamp_resolve->copy_to_buffer(encoder, 0, *m_timestamp_readback_buffer[i], 0, size_2_uint64);
        m_ringbuffer_index_read = i;
        increment_index(m_ringbuffer_index_write);
    }
#ifdef QT_DEBUG
    else {
        m_dbg_dropped_measurement_count++;
        if (m_dbg_dropped_measurement_count == 100) {
            qWarning() << "WebGPUTimer" << this->get_id() << "already dropped 100 measurements. Consider increasing ring buffer size.";
        }
    }
#endif
}

void WebGpuTimer::resolve()
{
    if (m_ringbuffer_index_read < 0)
        return; // Nothing to resolve
    m_timestamp_readback_buffer[m_ringbuffer_index_read]->read_back_async(m_device, [this](WGPUMapAsyncStatus status, std::vector<uint64_t> data) {
        if (status == WGPUMapAsyncStatus_Success) {
            const float result_in_s = (data[1] - data[0]) / 1e9;
            add_result(result_in_s);
        }
    });
    m_ringbuffer_index_read = -1;
}

} // namespace webgpu::timing

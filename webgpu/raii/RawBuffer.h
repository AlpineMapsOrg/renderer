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

#include "base_types.h"
#include <QDebug>
#include <QString>
#include <queue>
#include <webgpu/util/string_cast.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_interface.hpp>

namespace webgpu::raii {

/// Generic class for GPU buffer handles complying to RAII.
/// This class does not store the value to be written on CPU side.
template <typename T> class RawBuffer : public GpuResource<WGPUBuffer, WGPUBufferDescriptor, WGPUDevice> {
public:
    using ReadBackCallback = std::function<void(WGPUMapAsyncStatus, std::vector<T>)>;

    struct ReadBackState {
        ReadBackCallback callback;
        std::unique_ptr<raii::RawBuffer<T>> staging_buffer;
    };

    // m_size in num objects
    RawBuffer(WGPUDevice device, WGPUBufferUsage usage, size_t size, const std::string& label = "label not set")
        : GpuResource(device,
              WGPUBufferDescriptor {
                  .nextInChain = nullptr,
                  .label = WGPUStringView { .data = label.c_str(), .length = WGPU_STRLEN },
                  .usage = usage,
                  .size = size * sizeof(T),
                  .mappedAtCreation = false,
              })
        , m_size(size)
    {
    }

    // count and offset in number of elements of size sizeof(T)
    void write(WGPUQueue queue, const T* data, size_t count = 1, size_t offset = 0)
    {
        assert(count <= m_size);
        wgpuQueueWriteBuffer(queue, m_handle, offset * sizeof(T), data, count * sizeof(T)); // takes size in bytes
    }

    void clear(WGPUDevice device, WGPUQueue queue)
    {
        // bind GPU resources and run pipeline
        WGPUCommandEncoderDescriptor descriptor {};
        descriptor.label = WGPUStringView { .data = "buffer clear command encoder", .length = WGPU_STRLEN };
        webgpu::raii::CommandEncoder encoder(device, descriptor);

        clear(encoder.handle());

        WGPUCommandBufferDescriptor cmd_buffer_descriptor {};
        cmd_buffer_descriptor.label = WGPUStringView { .data = "buffer clear command buffer", .length = WGPU_STRLEN }; // TODO add buffer label here
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_descriptor);
        wgpuQueueSubmit(queue, 1, &command);
        wgpuCommandBufferRelease(command);
    }
    void clear(WGPUCommandEncoder encoder) { wgpuCommandEncoderClearBuffer(encoder, m_handle, 0, m_size * sizeof(T)); }
    void clear(WGPUCommandEncoder encoder, size_t count, size_t offset = 0) { wgpuCommandEncoderClearBuffer(encoder, m_handle, offset, count * sizeof(T)); }

    /// copy contents from this buffer into other buffer
    template <typename OtherT>
    void copy_to_buffer(WGPUCommandEncoder encoder, size_t src_offset_bytes, const raii::RawBuffer<OtherT>& dst, size_t dst_offset_bytes, size_t size_bytes)
    {
        wgpuCommandEncoderCopyBufferToBuffer(encoder, handle(), src_offset_bytes, dst.handle(), dst_offset_bytes, size_bytes);
    }
    template <typename OtherT>
    void copy_to_buffer(WGPUDevice device, size_t src_offset_bytes, const raii::RawBuffer<OtherT>& dst, size_t dst_offset_bytes, size_t size_bytes)
    {
        WGPUCommandEncoderDescriptor desc {};
        desc.label = WGPUStringView { .data = "copy texture to buffer command encoder", .length = WGPU_STRLEN };
        raii::CommandEncoder encoder(device, desc);

        copy_to_buffer<OtherT>(encoder.handle(), src_offset_bytes, dst, dst_offset_bytes, size_bytes);

        // submit to queue
        WGPUCommandBufferDescriptor cmd_buffer_desc {};
        WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_desc);
        WGPUQueue queue = wgpuDeviceGetQueue(device);
        wgpuQueueSubmit(queue, 1, &cmd_buffer);
        // TODO release cmd buffer -> use raii
    }

    /// Copy all contents of this buffer into other buffer
    template <typename OtherT> void copy_to_buffer(WGPUCommandEncoder encoder, const raii::RawBuffer<OtherT>& dst)
    {
        copy_to_buffer<OtherT>(encoder, 0, dst, 0, size_in_byte());
    }
    template <typename OtherT> void copy_to_buffer(WGPUDevice device, const raii::RawBuffer<OtherT>& dst)
    {
        copy_to_buffer<OtherT>(device, 0, dst, 0, size_in_byte());
    }

#ifndef __EMSCRIPTEN__
    // wgpuBufferGetMapState buggy on web, see https://github.com/weBIGeo/webigeo/issues/26#issuecomment-2259959378
    WGPUBufferMapState map_state() { return wgpuBufferGetMapState(handle()); }
#else
    WGPUBufferMapState map_state() { return m_buffer_mapping_state; }
#endif

    /// Read back buffer asynchronously. Callback is called by webGPU when buffer is mapped.
    WGPUFuture read_back_async(WGPUDevice device, ReadBackCallback callback)
    {
        auto on_buffer_mapped = [](WGPUMapAsyncStatus status, WGPUStringView message, void* user_data, [[maybe_unused]] void* user_data2) {
            RawBuffer<T>* _this = reinterpret_cast<RawBuffer<T>*>(user_data);
            const auto& callback_state = _this->m_read_back_callbacks.front();
            std::vector<T> buffer_data;

            if (status != WGPUMapAsyncStatus_Success) {
                // ToDo eventually caller should be in charge of error report
                qCritical() << "failed buffer mapping -" << webgpu::util::bufferMapAsyncStatusToString(status) << ", message: " << message.data;
            } else {
                WGPUBuffer buffer_handle = _this->descriptor().usage & WGPUBufferUsage_MapRead ? _this->handle() : callback_state.staging_buffer->handle();
                auto raw_buffer_data = static_cast<const T*>(wgpuBufferGetConstMappedRange(buffer_handle, 0, _this->size_in_byte()));
                buffer_data.insert(buffer_data.end(), raw_buffer_data, raw_buffer_data + _this->size());
                wgpuBufferUnmap(buffer_handle);
            }
#ifdef __EMSCRIPTEN__
            _this->m_buffer_mapping_state = WGPUBufferMapState_Unmapped;
#endif
            callback_state.callback(status, buffer_data);

            _this->m_read_back_callbacks.pop(); // also deletes staging buffer, if one was used
        };

        WGPUBufferMapCallbackInfo on_buffer_mapped_callback_info {
            .nextInChain = nullptr,
            .mode = WGPUCallbackMode_AllowProcessEvents,
            .callback = on_buffer_mapped,
            .userdata1 = this,
            .userdata2 = nullptr,
        };

        // if possible, maps buffer directly, otherwise creates staging buffer, copies to staging buffer and maps staging buffer
        if (descriptor().usage & WGPUBufferUsage_MapRead) { // can read directly
            m_read_back_callbacks.emplace(callback);
#ifdef __EMSCRIPTEN__
            m_buffer_mapping_state = WGPUBufferMapState_Pending;
#endif
            return wgpuBufferMapAsync(handle(), WGPUMapMode_Read, 0, size_in_byte(), on_buffer_mapped_callback_info);
        } else if (descriptor().usage & WGPUBufferUsage_CopySrc) {
            m_read_back_callbacks.emplace(callback,
                std::make_unique<raii::RawBuffer<T>>(device, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, size(), "buffer readback staging buffer"));
            copy_to_buffer(device, *(m_read_back_callbacks.back().staging_buffer));
            return wgpuBufferMapAsync(
                m_read_back_callbacks.back().staging_buffer->handle(), WGPUMapMode_Read, 0, size_in_byte(), on_buffer_mapped_callback_info);
        } else {
            qFatal("Cannot initialise buffer read back. Buffer requires MapRead or CopySrc usage");
            return {};
        }
    }

    /// Read back buffer synchronously. Blocks until buffer is mapped and read back but at most max_timeout_ms.
    WGPUMapAsyncStatus read_back_sync(WGPUInstance instance, WGPUDevice device, std::vector<T>& result, uint32_t max_timeout_ms = 1000)
    {
        WGPUMapAsyncStatus return_status = WGPUMapAsyncStatus_Error;
        bool work_done = false;

        WGPUFuture read_back_future = read_back_async(device, [&return_status, &result, &work_done](WGPUMapAsyncStatus status, std::vector<T> async_buffer) {
            return_status = status;
            if (status == WGPUMapAsyncStatus_Success) {
                result.swap(async_buffer);
            }
            work_done = true;
        });

        WGPUFutureWaitInfo wait_info { .future = read_back_future, .completed = false };
        WGPUWaitStatus wait_status = wgpuInstanceWaitAny(instance, 1, &wait_info, max_timeout_ms * 1000 * 1000);
        if (wait_status != WGPUWaitStatus_Success) {
            qCritical() << "Failed to map buffer, WGPUWaitStatus was " << wait_status;
        }

        return return_status;
    }

    size_t size() const { return m_size; }
    size_t size_in_byte() const { return m_size * sizeof(T); };

    WGPUBindGroupEntry create_bind_group_entry(uint32_t binding) const
    {
        WGPUBindGroupEntry entry {};
        entry.binding = binding;
        entry.buffer = m_handle;
        entry.size = size_in_byte();
        entry.offset = 0;
        entry.nextInChain = nullptr;
        return entry;
    }

private:
    size_t m_size;
    std::queue<ReadBackState> m_read_back_callbacks;

#ifdef __EMSCRIPTEN__
    WGPUBufferMapState m_buffer_mapping_state = WGPUBufferMapState_Unmapped;
#endif
};

} // namespace webgpu::raii

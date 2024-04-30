/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "Buffer.h"
#include "TextureView.h"
#include "base_types.h"
#include "nucleus/Raster.h"
#include "nucleus/utils/ColourTexture.h"
#include <QImage>
#include <map>
#include <webgpu/webgpu.h>

namespace webgpu_engine::raii {

/// Represents (web)GPU texture.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
/// Preferably to be used with std::unique_ptr or std::shared_ptr.
class Texture : public GpuResource<WGPUTexture, WGPUTextureDescriptor, WGPUDevice> {
public:
    static const std::map<QImage::Format, WGPUTextureFormat> qimage_to_webgpu_format;
    static const std::map<WGPUTextureFormat, size_t> bytes_per_element;

public:
    using GpuResource::GpuResource;

    void write(WGPUQueue queue, QImage image, uint32_t layer = 0);

    // TODO could make this a function template and pass type instead of using uint16_t? but not needed rn
    void write(WGPUQueue queue, const nucleus::Raster<uint16_t>& data, uint32_t layer = 0);

    void write(WGPUQueue queue, const nucleus::utils::ColourTexture& data, uint32_t layer = 0);

    // submits to default queue
    template <typename T> void copy_to_buffer(WGPUDevice device, const raii::RawBuffer<T>& buffer, uint32_t layer = 0) const;

    template <typename T> void copy_to_buffer(WGPUCommandEncoder encoder, const raii::RawBuffer<T>& buffer, uint32_t layer = 0) const;

    WGPUTextureViewDescriptor default_texture_view_descriptor() const;

    std::unique_ptr<TextureView> create_view() const;
    std::unique_ptr<TextureView> create_view(const WGPUTextureViewDescriptor& desc) const;
};

template <typename T> void Texture::copy_to_buffer(WGPUDevice device, const raii::RawBuffer<T>& buffer, uint32_t layer) const
{
    WGPUCommandEncoderDescriptor desc {};
    desc.label = "copy texture to buffer command encoder";
    raii::CommandEncoder encoder(device, desc);
    copy_to_buffer<T>(encoder.handle(), buffer, layer);
    WGPUCommandBufferDescriptor cmd_buffer_desc {};
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_desc);
    WGPUQueue queue = wgpuDeviceGetQueue(device);
    wgpuQueueSubmit(queue, 1, &cmd_buffer);
    // TODO release cmd buffer -> use raii
}

template <typename T> void Texture::copy_to_buffer(WGPUCommandEncoder encoder, const raii::RawBuffer<T>& buffer, uint32_t layer) const
{
    WGPUImageCopyTexture source {};
    source.texture = m_handle;
    source.mipLevel = 0;
    source.origin = { .x = 0, .y = 0, .z = layer };
    source.aspect = WGPUTextureAspect_All;

    WGPUImageCopyBuffer destination {};
    destination.buffer = buffer.handle();
    destination.layout.offset = 0;
    destination.layout.bytesPerRow = m_descriptor.size.width * bytes_per_element.at(m_descriptor.format);
    destination.layout.rowsPerImage = m_descriptor.size.height;

    const WGPUExtent3D extent { .width = m_descriptor.size.width, .height = m_descriptor.size.height, .depthOrArrayLayers = 1 };
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &extent);
}

} // namespace webgpu_engine::raii

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

#include "RawBuffer.h"
#include "TextureView.h"
#include "base_types.h"
#include "nucleus/Raster.h"
#include "nucleus/utils/ColourTexture.h"
#include <webgpu/webgpu.h>

namespace webgpu::raii {

/// Represents (web)GPU texture.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
/// Preferably to be used with std::unique_ptr or std::shared_ptr.
class Texture : public GpuResource<WGPUTexture, WGPUTextureDescriptor, WGPUDevice> {
public:
    using ReadBackCallback = std::function<void(size_t layer_index, std::shared_ptr<QByteArray>)>;

    struct ReadBackState {
        const Texture* texture;
        std::unique_ptr<RawBuffer<char>> buffer;
        ReadBackCallback callback;
        size_t layer_index;
    };

    static uint8_t get_bytes_per_element(WGPUTextureFormat format);

    static const uint16_t BYTES_PER_ROW_PADDING;

public:
    using GpuResource::GpuResource;

    template <typename RasterElementT> void write(WGPUQueue queue, const nucleus::Raster<RasterElementT>& data, uint32_t layer = 0)
    {
        // TODO maybe assert if RasterElementT and WGPUTextureFormat of this texture are compatible?

        assert(static_cast<uint32_t>(data.width()) == m_descriptor.size.width);
        assert(static_cast<uint32_t>(data.height()) == m_descriptor.size.height);

        WGPUTexelCopyTextureInfo texel_copy_texture_info {};
        texel_copy_texture_info.texture = m_handle;
        texel_copy_texture_info.mipLevel = 0;
        texel_copy_texture_info.origin = WGPUOrigin3D { 0, 0, layer };
        texel_copy_texture_info.aspect = WGPUTextureAspect::WGPUTextureAspect_All;

        WGPUTexelCopyBufferLayout texture_data_layout_info {};
        texture_data_layout_info.offset = 0;
        texture_data_layout_info.bytesPerRow = uint32_t(sizeof(RasterElementT) * data.width());
        texture_data_layout_info.rowsPerImage = uint32_t(data.height());

        WGPUExtent3D copy_extent { m_descriptor.size.width, m_descriptor.size.height, 1 };

        wgpuQueueWriteTexture(queue, &texel_copy_texture_info, data.bytes(), uint32_t(data.size_in_bytes()), &texture_data_layout_info, &copy_extent);
    }

    void write(WGPUQueue queue, const nucleus::utils::ColourTexture& data, uint32_t layer = 0);

    // submits to default queue of device
    template <typename T>
    void copy_to_buffer(WGPUDevice device, const RawBuffer<T>& buffer, glm::uvec3 origin = glm::uvec3(0), glm::uvec2 extent = glm::uvec2(0)) const
    {
        WGPUCommandEncoderDescriptor desc {};
        desc.label = WGPUStringView { .data = "copy texture to buffer command encoder", .length = WGPU_STRLEN };
        raii::CommandEncoder encoder(device, desc);
        copy_to_buffer<T>(encoder.handle(), buffer, origin, extent);
        WGPUCommandBufferDescriptor cmd_buffer_desc {};
        WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder.handle(), &cmd_buffer_desc);
        WGPUQueue queue = wgpuDeviceGetQueue(device);
        wgpuQueueSubmit(queue, 1, &cmd_buffer);
        // TODO release cmd buffer -> use raii
    }

    template <typename T>
    void copy_to_buffer(WGPUCommandEncoder encoder, const RawBuffer<T>& buffer, glm::uvec3 origin = glm::uvec3(0), glm::uvec2 extent = glm::uvec2(0)) const
    {
        if (extent.x == 0 || extent.y == 0) {
            extent = glm::uvec2(m_descriptor.size.width, m_descriptor.size.height);
        }

        // the row for the destination buffer needs to be aligned to 256 byte. Meaning: If we have
        // a texture that does not fit those requirements we need to have a buffer with appropriate
        // padding per row.
        uint32_t bytes_per_extent_row = uint32_t(
            std::ceil(double(extent.x) * double(get_bytes_per_element(m_descriptor.format)) / double(BYTES_PER_ROW_PADDING)) * BYTES_PER_ROW_PADDING);

        assert(bytes_per_extent_row * extent.y <= buffer.size_in_byte());

        WGPUTexelCopyTextureInfo source {};
        source.texture = m_handle;
        source.mipLevel = 0;
        source.origin = { .x = origin.x, .y = origin.y, .z = origin.z };
        source.aspect = WGPUTextureAspect_All;

        WGPUTexelCopyBufferInfo destination {};
        destination.buffer = buffer.handle();
        destination.layout.offset = 0;
        destination.layout.bytesPerRow = bytes_per_extent_row; // this has to be a multiple of 256
        destination.layout.rowsPerImage = extent.y;

        const WGPUExtent3D wgpu_extent { .width = extent.x, .height = extent.y, .depthOrArrayLayers = 1 };

        wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &wgpu_extent);
    }

    void copy_to_texture(WGPUCommandEncoder encoder, uint32_t source_layer, const Texture& target_texture, uint32_t target_layer = 0) const;

    /// read back single texture layer of this texture
    void read_back_async(WGPUDevice device, size_t layer_index, ReadBackCallback callback) const;

    /// should only be used for debugging purposes
    void save_to_file(WGPUDevice device, const std::string& filename, size_t layer_index = 0);

    WGPUTextureViewDescriptor default_texture_view_descriptor() const;

    std::unique_ptr<TextureView> create_view() const;
    std::unique_ptr<TextureView> create_view(const WGPUTextureViewDescriptor& desc) const;

    size_t width() const;
    size_t height() const;
    size_t depth_or_num_layers() const;
    uint32_t mip_level_count() const;
    size_t size_in_bytes() const;
    size_t bytes_per_row() const;
    size_t single_layer_size_in_bytes() const;

};

} // namespace webgpu::raii

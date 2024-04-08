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

#include "Texture.h"

#include "TextureView.h"

namespace webgpu_engine::raii {

// TODO
const std::map<QImage::Format, WGPUTextureFormat> Texture::qimage_to_webgpu_format { { QImage::Format::Format_RGBA8888,
    WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm } };

void Texture::write(WGPUQueue queue, QImage image, uint32_t layer)
{
    // assert dimensions and format match up
    assert(static_cast<uint32_t>(image.width()) == m_descriptor.size.width);
    assert(static_cast<uint32_t>(image.height()) == m_descriptor.size.height);
    assert(m_descriptor.format == qimage_to_webgpu_format.at(image.format())); // TODO could also just convert to other format

    WGPUImageCopyTexture image_copy_texture {};
    image_copy_texture.texture = m_handle;
    image_copy_texture.aspect = WGPUTextureAspect::WGPUTextureAspect_All;
    image_copy_texture.mipLevel = 0;
    image_copy_texture.origin = { 0, 0, layer };
    image_copy_texture.nextInChain = nullptr;

    WGPUTextureDataLayout texture_data_layout {};
    texture_data_layout.bytesPerRow = image.bytesPerLine();
    texture_data_layout.rowsPerImage = image.height();
    texture_data_layout.offset = 0;
    texture_data_layout.nextInChain = nullptr;

    WGPUExtent3D copy_extent { m_descriptor.size.width, m_descriptor.size.height, 1 };
    wgpuQueueWriteTexture(queue, &image_copy_texture, image.bits(), image.sizeInBytes(), &texture_data_layout, &copy_extent);
}

void Texture::write(WGPUQueue queue, const nucleus::Raster<uint16_t>& data, uint32_t layer)
{
    assert(static_cast<uint32_t>(data.width()) == m_descriptor.size.width);
    assert(static_cast<uint32_t>(data.height()) == m_descriptor.size.height);

    WGPUImageCopyTexture image_copy_texture {};
    image_copy_texture.texture = m_handle;
    image_copy_texture.aspect = WGPUTextureAspect::WGPUTextureAspect_All;
    image_copy_texture.mipLevel = 0;
    image_copy_texture.origin = { 0, 0, layer };
    image_copy_texture.nextInChain = nullptr;

    WGPUTextureDataLayout texture_data_layout {};
    texture_data_layout.bytesPerRow = uint32_t(sizeof(uint16_t) * data.width());
    texture_data_layout.rowsPerImage = uint32_t(data.height());
    texture_data_layout.offset = 0;
    texture_data_layout.nextInChain = nullptr;
    WGPUExtent3D copy_extent { m_descriptor.size.width, m_descriptor.size.height, 1 };
    // TODO maybe add buffer_length_in_bytes() to Raster
    wgpuQueueWriteTexture(queue, &image_copy_texture, data.bytes(), uint32_t(sizeof(uint16_t) * data.buffer_length()), &texture_data_layout, &copy_extent);
}

void Texture::write(WGPUQueue queue, const nucleus::utils::ColourTexture& data, uint32_t layer)
{
    assert(static_cast<uint32_t>(data.width()) == m_descriptor.size.width);
    assert(static_cast<uint32_t>(data.height()) == m_descriptor.size.height);
    assert(data.format() == nucleus::utils::ColourTexture::Format::Uncompressed_RGBA); // TODO compressed textures

    WGPUImageCopyTexture image_copy_texture {};
    image_copy_texture.texture = m_handle;
    image_copy_texture.aspect = WGPUTextureAspect::WGPUTextureAspect_All;
    image_copy_texture.mipLevel = 0;
    image_copy_texture.origin = { 0, 0, layer };
    image_copy_texture.nextInChain = nullptr;

    WGPUTextureDataLayout texture_data_layout {};
    texture_data_layout.bytesPerRow = 4 * data.width(); // for uncompressed RGBA
    texture_data_layout.rowsPerImage = data.height();
    texture_data_layout.offset = 0;
    texture_data_layout.nextInChain = nullptr;
    WGPUExtent3D copy_extent { m_descriptor.size.width, m_descriptor.size.height, 1 };
    wgpuQueueWriteTexture(queue, &image_copy_texture, data.data(), data.n_bytes(), &texture_data_layout, &copy_extent);
}

WGPUTextureViewDescriptor Texture::default_texture_view_descriptor() const
{
    // TODO make utility function
    auto determineViewDimension = [](const WGPUTextureDescriptor& texture_desc) {
        if (texture_desc.dimension == WGPUTextureDimension_1D)
            return WGPUTextureViewDimension_1D;
        else if (texture_desc.dimension == WGPUTextureDimension_3D)
            return WGPUTextureViewDimension_3D;
        else if (texture_desc.dimension == WGPUTextureDimension_2D) {
            return texture_desc.size.depthOrArrayLayers > 1 ? WGPUTextureViewDimension_2DArray : WGPUTextureViewDimension_2D;
            // note: if texture_desc.size.depthOrArrayLayers is 6, the view type can also be WGPUTextureViewDimension_Cube
            //       or, for any multiple of 6 the view type could be WGPUTextureViewDimension_CubeArray - we don't support this here for now
        }
        return WGPUTextureViewDimension_Undefined; // hopefully this logs an error when webgpu is validating
    };

    WGPUTextureViewDescriptor view_desc {};
    view_desc.aspect = WGPUTextureAspect_All;
    view_desc.dimension = determineViewDimension(m_descriptor);
    view_desc.format = m_descriptor.format;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = m_descriptor.size.depthOrArrayLayers;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = m_descriptor.mipLevelCount;
    return view_desc;
}

std::unique_ptr<TextureView> Texture::create_view() const { return create_view(default_texture_view_descriptor()); }

std::unique_ptr<TextureView> Texture::create_view(const WGPUTextureViewDescriptor& desc) const { return std::make_unique<TextureView>(m_handle, desc); }

} // namespace webgpu_engine::raii

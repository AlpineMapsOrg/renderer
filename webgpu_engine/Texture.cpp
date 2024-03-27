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

namespace webgpu_engine {

// TODO
const std::map<QImage::Format, WGPUTextureFormat> Texture::qimage_to_webgpu_format { { QImage::Format::Format_RGBA8888,
    WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm } };

Texture::Texture(WGPUDevice device, const WGPUTextureDescriptor& texture_desc)
    : m_texture(wgpuDeviceCreateTexture(device, &texture_desc))
    , m_texture_desc(texture_desc)
{
}

Texture::~Texture()
{
    wgpuTextureDestroy(m_texture);
    wgpuTextureRelease(m_texture);
}

void Texture::write(WGPUQueue queue, QImage image, uint32_t layer)
{
    // assert dimensions and format match up
    assert(static_cast<uint32_t>(image.width()) == m_texture_desc.size.width);
    assert(static_cast<uint32_t>(image.height()) == m_texture_desc.size.height);
    assert(m_texture_desc.format == qimage_to_webgpu_format.at(image.format())); // TODO could also just convert to other format

    WGPUImageCopyTexture image_copy_texture {};
    image_copy_texture.texture = m_texture;
    image_copy_texture.aspect = WGPUTextureAspect::WGPUTextureAspect_All;
    image_copy_texture.mipLevel = 0;
    image_copy_texture.origin = { 0, 0, layer };
    image_copy_texture.nextInChain = nullptr;

    WGPUTextureDataLayout texture_data_layout {};
    texture_data_layout.bytesPerRow = image.bytesPerLine();
    texture_data_layout.rowsPerImage = image.height();
    texture_data_layout.offset = 0;
    texture_data_layout.nextInChain = nullptr;

    WGPUExtent3D copy_extent { m_texture_desc.size.width, m_texture_desc.size.height, 1 };
    wgpuQueueWriteTexture(queue, &image_copy_texture, image.bits(), image.sizeInBytes(), &texture_data_layout, &copy_extent);
}

std::unique_ptr<TextureView> Texture::create_view(const WGPUTextureViewDescriptor& desc) const
{
    return std::make_unique<webgpu_engine::TextureView>(m_texture, desc);
}

TextureView::TextureView(WGPUTexture texture_handle, const WGPUTextureViewDescriptor& desc)
    : m_texture_view(wgpuTextureCreateView(texture_handle, &desc))
    , m_texture_view_descriptor(desc)
{
}

TextureView::~TextureView() { wgpuTextureViewRelease(m_texture_view); }

WGPUTextureViewDimension TextureView::dimension() const { return m_texture_view_descriptor.dimension; }

WGPUTextureView TextureView::handle() const { return m_texture_view; }

Sampler::Sampler(WGPUDevice device, const WGPUSamplerDescriptor& desc)
    : m_sampler(wgpuDeviceCreateSampler(device, &desc))
{
}

Sampler::~Sampler() { wgpuSamplerRelease(m_sampler); }

WGPUSampler Sampler::handle() const { return m_sampler; }

TextureWithSampler::TextureWithSampler(WGPUDevice device, const WGPUTextureDescriptor& texture_desc, const WGPUSamplerDescriptor& sampler_desc)
    : m_texture(std::make_unique<Texture>(device, texture_desc))
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
    view_desc.dimension = determineViewDimension(texture_desc);
    view_desc.format = texture_desc.format;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = texture_desc.size.depthOrArrayLayers;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = texture_desc.mipLevelCount;
    m_texture_view = m_texture->create_view(view_desc);
    m_sampler = std::make_unique<Sampler>(device, sampler_desc);
}

Texture& TextureWithSampler::texture() { return *m_texture; }

const TextureView& TextureWithSampler::texture_view() const { return *m_texture_view; }

const Sampler& TextureWithSampler::sampler() const { return *m_sampler; }

} // namespace webgpu_engine

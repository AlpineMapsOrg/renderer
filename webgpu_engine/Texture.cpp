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
const std::map<QImage::Format, wgpu::TextureFormat> Texture::qimage_to_webgpu_format { { QImage::Format::Format_RGBA8888, wgpu::TextureFormat::RGBA8Unorm },
    { QImage::Format::Format_RGB888, wgpu::TextureFormat::ETC2RGB8Unorm }, { QImage::Format::Format_RGBA32FPx4, wgpu::TextureFormat::RGBA32Float } };

Texture::Texture(wgpu::Device device, wgpu::TextureDescriptor texture_desc)
    : m_texture(device.createTexture(texture_desc))
    , m_texture_desc(texture_desc)
{
}

Texture::~Texture()
{
    m_texture.destroy();
    m_texture.release();
}

void Texture::write(wgpu::Queue queue, QImage image, uint32_t layer)
{
    // assert dimensions and format match up
    assert(static_cast<uint32_t>(image.width()) == m_texture_desc.size.width);
    assert(static_cast<uint32_t>(image.height()) == m_texture_desc.size.height);
    assert(m_texture_desc.format == qimage_to_webgpu_format.at(image.format())); // TODO could also just convert to other format

    wgpu::ImageCopyTexture image_copy_texture {};
    image_copy_texture.texture = m_texture;
    image_copy_texture.aspect = wgpu::TextureAspect::All;
    image_copy_texture.mipLevel = 0;
    image_copy_texture.origin = { 0, 0, layer };
    image_copy_texture.nextInChain = nullptr;

    wgpu::TextureDataLayout texture_data_layout {};
    texture_data_layout.bytesPerRow = image.bytesPerLine();
    texture_data_layout.rowsPerImage = image.height();
    texture_data_layout.offset = 0;
    texture_data_layout.nextInChain = nullptr;

    wgpu::Extent3D copy_extent { m_texture_desc.size.width, m_texture_desc.size.height, 1 };
    queue.writeTexture(image_copy_texture, image.bits(), image.sizeInBytes(), texture_data_layout, copy_extent);
}

std::unique_ptr<TextureView> Texture::create_view(const wgpu::TextureViewDescriptor& desc)
{
    return std::make_unique<webgpu_engine::TextureView>(m_texture, desc);
}

TextureView::TextureView(wgpu::Texture& texture_handle, const wgpu::TextureViewDescriptor& desc)
    : m_texture_view(texture_handle.createView(desc))
    , m_texture_view_descriptor(desc)
{
}

TextureView::~TextureView() { m_texture_view.release(); }

wgpu::TextureViewDimension TextureView::dimension() const { return m_texture_view_descriptor.dimension; }

wgpu::TextureView TextureView::handle() const { return m_texture_view; }

Sampler::Sampler(wgpu::Device device, const wgpu::SamplerDescriptor& desc)
    : m_sampler(device.createSampler(desc))
    , m_sampler_descriptor(desc)
{
}

Sampler::~Sampler() { m_sampler.release(); }

wgpu::Sampler Sampler::handle() const { return m_sampler; }

} // namespace webgpu_engine

/*****************************************************************************
 * weBIGeo
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

#include "GpuTileStorage.h"

#include "nucleus/tile/conversion.h"
#include "nucleus/utils/image_loader.h"

namespace webgpu_engine::compute {

TileStorageTexture::TileStorageTexture(WGPUDevice device, WGPUTextureDescriptor texture_desc, WGPUSamplerDescriptor sampler_desc)
    : m_device { device }
    , m_queue { wgpuDeviceGetQueue(device) }
    , m_resolution { texture_desc.size.width, texture_desc.size.height }
    , m_capacity { texture_desc.size.depthOrArrayLayers }
    , m_layers_used(m_capacity, false)
{
    m_texture_array = std::make_unique<webgpu::raii::TextureWithSampler>(m_device, texture_desc, sampler_desc);
}

TileStorageTexture::TileStorageTexture(WGPUDevice device, const glm::uvec2& resolution, size_t capacity, WGPUTextureFormat format, WGPUTextureUsage usage)
    : webgpu_engine::compute::TileStorageTexture(
          device, create_default_texture_descriptor(resolution, capacity, format, usage), create_default_sampler_descriptor())
{
}

void TileStorageTexture::store(size_t layer, const QByteArray& data)
{
    assert(layer < m_capacity);

    // convert to raster and store in texture array
    const nucleus::Raster<glm::u8vec4> height_image = nucleus::utils::image_loader::rgba8(data).value();
    const auto heightraster = nucleus::tile::conversion::to_u16raster(height_image);
    m_texture_array->texture().write(m_queue, heightraster, uint32_t(layer));

    set_layer_used(layer);
}

size_t TileStorageTexture::store(const QByteArray& data)
{
    size_t layer_index = find_unused_layer_index();
    store(layer_index, data);
    return layer_index;
}

void TileStorageTexture::reserve(size_t layer)
{
    assert(!m_layers_used[layer]);

    set_layer_used(layer);
}

size_t TileStorageTexture::reserve()
{
    size_t layer_index = find_unused_layer_index();
    set_layer_used(layer_index);
    return layer_index;
}

void TileStorageTexture::clear()
{
    m_num_stored = 0;
    m_layers_used.clear();
    m_layers_used.resize(m_capacity, false);
}

void TileStorageTexture::clear(size_t layer)
{
    assert(layer < m_capacity);

    // update used layers
    if (m_layers_used.at(layer)) {
        m_num_stored--;
        m_layers_used[layer] = false;
    }
}

size_t TileStorageTexture::width() const { return m_texture_array->texture().descriptor().size.width; }

size_t TileStorageTexture::height() const { return m_texture_array->texture().descriptor().size.height; }

size_t TileStorageTexture::num_used() const { return used_layer_indices().size(); }

size_t TileStorageTexture::capacity() const { return m_capacity; }

std::vector<uint32_t> TileStorageTexture::used_layer_indices() const
{
    std::vector<uint32_t> indices;
    indices.reserve(m_capacity);
    for (uint32_t i = 0; i < m_capacity; i++) {
        if (m_layers_used.at(i)) {
            indices.emplace_back(i);
        }
    }
    return indices;
}

webgpu::raii::TextureWithSampler& TileStorageTexture::texture() { return *m_texture_array; }

const webgpu::raii::TextureWithSampler& TileStorageTexture::texture() const { return *m_texture_array; }

size_t TileStorageTexture::find_unused_layer_index() const
{
    assert(m_num_stored < m_capacity);

    auto found_at = std::find(m_layers_used.begin(), m_layers_used.end(), false);
    return found_at - m_layers_used.begin();
}

void TileStorageTexture::set_layer_used(size_t layer)
{
    // update used layers
    if (!m_layers_used.at(layer)) {
        m_num_stored++;
        m_layers_used[layer] = true;
    }
}

WGPUTextureDescriptor TileStorageTexture::create_default_texture_descriptor(
    const glm::uvec2& resolution, size_t capacity, WGPUTextureFormat format, WGPUTextureUsage usage)
{
    WGPUTextureDescriptor texture_desc {};
    texture_desc.label = WGPUStringView { .data = "compute storage texture", .length = WGPU_STRLEN };
    texture_desc.dimension = WGPUTextureDimension::WGPUTextureDimension_2D;
    texture_desc.size = { uint32_t(resolution.x), uint32_t(resolution.y), uint32_t(capacity) };
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    texture_desc.format = format;
    texture_desc.usage = usage;
    return texture_desc;
}

WGPUSamplerDescriptor TileStorageTexture::create_default_sampler_descriptor()
{
    WGPUSamplerDescriptor sampler_desc {};
    sampler_desc.label = WGPUStringView { .data = "compute storage sampler", .length = WGPU_STRLEN };
    sampler_desc.addressModeU = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode::WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    sampler_desc.minFilter = WGPUFilterMode::WGPUFilterMode_Nearest;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode::WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1.0f;
    sampler_desc.compare = WGPUCompareFunction::WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;
    return sampler_desc;
}

} // namespace webgpu_engine::compute

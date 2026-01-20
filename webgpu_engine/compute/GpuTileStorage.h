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

#pragma once

#include <webgpu/raii/TextureWithSampler.h>

namespace webgpu_engine::compute {

/// Minimal wrapper over texture array for more convenient usage (intended for storing tile textures).
class TileStorageTexture {

public:
    TileStorageTexture(WGPUDevice device, WGPUTextureDescriptor texture_desc, WGPUSamplerDescriptor sampler_desc);

    // convenience wrapper
    TileStorageTexture(WGPUDevice device,
        const glm::uvec2& resolution,
        size_t capacity,
        WGPUTextureFormat format,
        WGPUTextureUsage usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);

    void store(size_t layer, const QByteArray& data);
    size_t store(const QByteArray& data); // store at next free spot
    void reserve(size_t layer); // acts like store, but doesnt write anything (useful for reservering on CPU side and writing to indices from shader)
    size_t reserve(); // reserve next free spot
    void clear(); // clear all
    void clear(size_t layer);

    size_t width() const;
    size_t height() const;
    size_t num_used() const;
    size_t capacity() const;
    std::vector<uint32_t> used_layer_indices() const;

    webgpu::raii::TextureWithSampler& texture();
    const webgpu::raii::TextureWithSampler& texture() const;

private:
    size_t find_unused_layer_index() const;
    void set_layer_used(size_t layer);

    static WGPUTextureDescriptor create_default_texture_descriptor(
        const glm::uvec2& resolution, size_t capacity, WGPUTextureFormat format, WGPUTextureUsage usage);
    static WGPUSamplerDescriptor create_default_sampler_descriptor();

private:
    WGPUDevice m_device;
    WGPUQueue m_queue;
    glm::uvec2 m_resolution;
    size_t m_capacity;
    size_t m_num_stored = 0; // number of stored textures
    std::vector<bool> m_layers_used; // CPU buffer for tracking which layers are currently used
    std::unique_ptr<webgpu::raii::TextureWithSampler> m_texture_array;
};

} // namespace webgpu_engine::compute

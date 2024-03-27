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

#include <QImage.h>
#include <map>
#include <webgpu/webgpu.h>

namespace webgpu_engine {

// forward declarations
class Texture;
class TextureView;

/// Represents (web)GPU texture.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
/// Preferably to be used with std::unique_ptr or std::shared_ptr.
class Texture {
public:
    static const std::map<QImage::Format, WGPUTextureFormat> qimage_to_webgpu_format;

public:
    Texture(WGPUDevice device, const WGPUTextureDescriptor& texture_desc);
    ~Texture();

    // delete copy constructor and copy-assignment operator
    Texture(const Texture& other) = delete;
    Texture& operator=(const Texture& other) = delete;

    void write(WGPUQueue queue, QImage image, uint32_t layer = 0);

    std::unique_ptr<TextureView> create_view(const WGPUTextureViewDescriptor& desc) const;

private:
    WGPUTexture m_texture;
    WGPUTextureDescriptor m_texture_desc; // for debugging
};

/// Represents (web)GPU texture view, that can be used as entry in a bind group.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
/// Preferably to be used with std::unique_ptr or std::shared_ptr.
/// Usually obtained via call to Texture::create_view rather than using constructor directly.
///
/// TODO decide whether RAII is needed for WGPUTextureView objects
class TextureView {
public:
    TextureView(WGPUTexture texture_handle, const WGPUTextureViewDescriptor& desc);
    ~TextureView();

    // delete copy constructor and copy-assignment operator
    TextureView(const TextureView& other) = delete;
    TextureView& operator=(const TextureView& other) = delete;

    WGPUTextureViewDimension dimension() const;
    WGPUTextureView handle() const;

private:
    WGPUTextureView m_texture_view;
    WGPUTextureViewDescriptor m_texture_view_descriptor;
};

/// Represents a (web)GPU sampler object, used to sample from a texture view in a shader.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
///
/// TODO decide whether RAII is needed for WGPUTextureView objects
class Sampler {
public:
    Sampler(WGPUDevice device, const WGPUSamplerDescriptor& desc);
    ~Sampler();

    // delete copy constructor and copy-assignment operator
    Sampler(const Sampler& other) = delete;
    Sampler& operator=(const Sampler& other) = delete;

    WGPUSampler handle() const;

private:
    WGPUSampler m_sampler;
};

class TextureWithSampler {
public:
    TextureWithSampler(WGPUDevice device, const WGPUTextureDescriptor& texture_desc, const WGPUSamplerDescriptor& sampler_desc);

    Texture& texture();
    const TextureView& texture_view() const;
    const Sampler& sampler() const;

private:
    std::unique_ptr<Texture> m_texture;
    std::unique_ptr<TextureView> m_texture_view;
    std::unique_ptr<Sampler> m_sampler;
};

} // namespace webgpu_engine

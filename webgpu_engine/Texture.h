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

#include "webgpu.hpp"

#include "QImage.h"

#include <map>

namespace webgpu_engine {

// forward declarations
class Texture;
class TextureView;

/// Represents (web)GPU texture.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
/// Preferably to be used with std::unique_ptr or std::shared_ptr.
class Texture {
public:
    static const std::map<QImage::Format, wgpu::TextureFormat> qimage_to_webgpu_format;

public:
    Texture(wgpu::Device device, wgpu::TextureDescriptor texture_desc);
    ~Texture();

    // delete copy constructor and copy-assignment operator
    Texture(const Texture& other) = delete;
    Texture& operator=(const Texture& other) = delete;

    void write(wgpu::Queue queue, QImage image, uint32_t layer = 0);

    // should be const, but couldnt call wgpu::Texture::createView then
    std::unique_ptr<TextureView> create_view(const wgpu::TextureViewDescriptor& desc);

private:
    wgpu::Texture m_texture;
    wgpu::TextureDescriptor m_texture_desc; // for debugging
};

/// Represents (web)GPU texture view, that can be used as entry in a bind group.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
/// Preferably to be used with std::unique_ptr or std::shared_ptr.
/// Usually obtained via call to Texture::create_view rather than using constructor directly.
///
/// TODO decide whether RAII is needed for wgpu::TextureView objects
///      if yes, to be consistent, we should add RAII wrappers for all wgpu::* classes
class TextureView {
public:
    TextureView(wgpu::Texture& texture_handle, const wgpu::TextureViewDescriptor& desc);
    ~TextureView();

    // delete copy constructor and copy-assignment operator
    TextureView(const TextureView& other) = delete;
    TextureView& operator=(const TextureView& other) = delete;

    wgpu::TextureViewDimension dimension() const;
    wgpu::TextureView handle() const;

private:
    wgpu::TextureView m_texture_view;
    wgpu::TextureViewDescriptor m_texture_view_descriptor;
};


/// Represents a (web)GPU sampler object, used to sample from a texture view in a shader.
/// Provides RAII semantics without ref-counting (free memory on deletion, disallow copy).
class Sampler {
public:
    Sampler(wgpu::Device device, const wgpu::SamplerDescriptor& desc);
    ~Sampler();

    // delete copy constructor and copy-assignment operator
    Sampler(const Sampler& other) = delete;
    Sampler& operator=(const Sampler& other) = delete;

    wgpu::Sampler handle() const;

private:
    wgpu::Sampler m_sampler;
    wgpu::SamplerDescriptor m_sampler_descriptor;
};

} // namespace webgpu_engine

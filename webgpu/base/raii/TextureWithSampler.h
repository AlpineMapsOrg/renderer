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

#include "Sampler.h"
#include "Texture.h"
#include "TextureView.h"
#include <webgpu/webgpu.h>

namespace webgpu::raii {

/// Convenience wrapper for a texture and a sampler sampling from the default view of that texture.
class TextureWithSampler {
public:
    TextureWithSampler(WGPUDevice device, const WGPUTextureDescriptor& texture_desc, const WGPUSamplerDescriptor& sampler_desc);

    Texture& texture();
    const Texture& texture() const;
    const TextureView& texture_view() const;
    const Sampler& sampler() const;

private:
    std::unique_ptr<Texture> m_texture;
    std::unique_ptr<TextureView> m_texture_view;
    std::unique_ptr<Sampler> m_sampler;
};

} // namespace webgpu::raii

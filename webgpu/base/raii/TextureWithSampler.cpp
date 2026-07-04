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

#include "TextureWithSampler.h"

namespace webgpu::raii {

TextureWithSampler::TextureWithSampler(WGPUDevice device, const WGPUTextureDescriptor& texture_desc, const WGPUSamplerDescriptor& sampler_desc)
    : m_texture(std::make_unique<Texture>(device, texture_desc))
    , m_texture_view(m_texture->create_view())
    , m_sampler(std::make_unique<Sampler>(device, sampler_desc))
{
}

Texture& TextureWithSampler::texture() { return *m_texture; }

const Texture& TextureWithSampler::texture() const { return *m_texture; }

const TextureView& TextureWithSampler::texture_view() const { return *m_texture_view; }

const Sampler& TextureWithSampler::sampler() const { return *m_sampler; }

} // namespace webgpu::raii

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

namespace webgpu_engine::raii {

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

} // namespace webgpu_engine::raii

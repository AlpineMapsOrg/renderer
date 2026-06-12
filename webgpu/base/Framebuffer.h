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

#include "raii/RenderPassEncoder.h"
#include "raii/Texture.h"
#include <memory>

namespace webgpu {

struct FramebufferFormat {
    glm::uvec2 size;
    WGPUTextureFormat depth_format;
    std::vector<WGPUTextureFormat> color_formats;
};

class Framebuffer {
public:
    Framebuffer(WGPUDevice device, const FramebufferFormat& format);

    void resize(const glm::uvec2& size);
    void recreate_depth_texture();
    void recreate_color_texture(size_t index);
    void recreate_all_textures();

    glm::uvec2 size() const;

    const raii::TextureView& color_texture_view(size_t index);
    const raii::Texture& color_texture(size_t index);

    const raii::TextureView& depth_texture_view();
    const raii::Texture& depth_texture();

    std::unique_ptr<raii::RenderPassEncoder> begin_render_pass(WGPUCommandEncoder encoder);

    // ToDo: Make generic for different types
    glm::vec4 read_colour_attachment_pixel(size_t index, const glm::dvec2& normalised_device_coordinates);

private:
    WGPUDevice m_device;
    FramebufferFormat m_format;
    std::unique_ptr<raii::Texture> m_depth_texture;
    std::unique_ptr<raii::TextureView> m_depth_texture_view;
    std::vector<std::unique_ptr<raii::Texture>> m_color_textures;
    std::vector<std::unique_ptr<raii::TextureView>> m_color_texture_views;
};

} // namespace webgpu

/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
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
#include <vector>

#include <QImage>
#include <glm/glm.hpp>


// There is QOpenGLFramebufferObject, but its API lacks the following:
// - get depth buffer as a texture
// - resize (while keeping all parametres)
// - limited depth / colour formats (to ease the pain of managing)

class QOpenGLTexture;

class Framebuffer
{
public:
    enum class DepthFormat {
        None,
        Int16,
        Int24,
        Float32
    };
    enum class ColourFormat {
        RGBA8,
        Float32
    };

private:
    DepthFormat m_depth_format;
    std::vector<ColourFormat> m_colour_formats;
    std::unique_ptr<QOpenGLTexture> m_depth_texture;
    std::unique_ptr<QOpenGLTexture> m_colour_texture;
    unsigned m_frame_buffer = -1;
    glm::uvec2 m_size;

public:
    Framebuffer(DepthFormat depth_format, std::vector<ColourFormat> colour_formats);
    ~Framebuffer();
    void resize(const glm::uvec2& new_size);
    void bind();
    void bind_colour_texture(unsigned index);
    std::unique_ptr<QOpenGLTexture> take_and_replace_colour_attachment(unsigned index);
    QImage read_colour_attachment(unsigned index);
    static void unbind();

private:
    void reset_fbo();
};


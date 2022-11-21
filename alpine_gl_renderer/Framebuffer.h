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
        RGBA8
    };

private:
    DepthFormat m_depth_format;
    std::vector<ColourFormat> m_colour_formats;
    unsigned m_frame_buffer;
public:
    unsigned m_frame_buffer_colour;
    unsigned m_frame_buffer_depth;

public:
    Framebuffer(DepthFormat depth_format, std::vector<ColourFormat> colour_formats);
    void resize(const glm::uvec2& new_size);
    void bind();
    QImage colour_attachment(unsigned index);
    static void unbind();

private:
    [[nodiscard]] int internal_format(ColourFormat) const;
    [[nodiscard]] int internal_format(DepthFormat) const;
    [[nodiscard]] int type(ColourFormat) const;
    [[nodiscard]] int type(DepthFormat) const;
};


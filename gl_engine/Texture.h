/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
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

#include <QImage>
#include <qopengl.h>

#include <nucleus/Raster.h>
#include <nucleus/utils/ColourTexture.h>

namespace gl_engine {
class Texture {
public:
    enum class Target : GLenum { _2d = GL_TEXTURE_2D, _2dArray = GL_TEXTURE_2D_ARRAY };
    enum class Format : GLenum { RGBA8 = GL_RGBA8, CompressedRGBA8 = GLenum(-2), RG8 = GL_RG8, R16UI = GL_R16UI, Invalid = GLenum(-1) };
    enum class Filter : GLint { Nearest = GL_NEAREST, Linear = GL_LINEAR, MipMapLinear = GL_LINEAR_MIPMAP_LINEAR };

public:
    Texture(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture& operator=(Texture&&) = delete;
    explicit Texture(Target target, Format format);
    ~Texture();

    void bind(unsigned texture_unit);
    void setParams(Filter min_filter, Filter mag_filter);
    void allocate_array(unsigned width, unsigned height, unsigned n_layers);
    void upload(const nucleus::utils::ColourTexture& texture);
    void upload(const nucleus::utils::ColourTexture& texture, unsigned array_index);
    void upload(const nucleus::Raster<glm::u8vec2>& texture);
    void upload(const nucleus::Raster<uint16_t>& texture);

    static GLenum compressed_texture_format();
    static nucleus::utils::ColourTexture::Format compression_algorithm();

private:
    GLuint m_id = GLuint(-1);
    Target m_target = Target::_2d;
    Format m_format = Format::Invalid;
    Filter m_min_filter = Filter::Nearest;
    Filter m_mag_filter = Filter::Nearest;
    unsigned m_width = unsigned(-1);
    unsigned m_height = unsigned(-1);
    unsigned m_n_layers = unsigned(-1);
};

} // namespace gl_engine

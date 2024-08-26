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
#include <cstdint>
#include <qopengl.h>
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif
#include <nucleus/Raster.h>
#include <nucleus/utils/ColourTexture.h>

namespace gl_engine {
class Texture {
public:
    enum class Target : GLenum { _2d = GL_TEXTURE_2D, _2dArray = GL_TEXTURE_2D_ARRAY }; // no 1D textures in webgl
    enum class Format {
        RGBA8, // normalised on gpu
        CompressedRGBA8, // normalised on gpu, compression format depends on desktop/mobile
        RGBA8UI,
        RG8, // normalised on gpu
        RG32UI,
        R16UI,
        R32UI,
        Invalid
    };
    enum class Filter : GLint { Nearest = GL_NEAREST, Linear = GL_LINEAR, MipMapLinear = GL_LINEAR_MIPMAP_LINEAR };

public:
    Texture(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture& operator=(Texture&&) = delete;
    explicit Texture(Target target, Format format);
    ~Texture();

    void bind(unsigned texture_unit);
    void setParams(Filter min_filter, Filter mag_filter, bool anisotropic_filtering = false);
    void allocate_array(unsigned width, unsigned height, unsigned n_layers);
    void upload(const nucleus::utils::ColourTexture& texture);
    void upload(const nucleus::utils::ColourTexture& texture, unsigned array_index);
    void upload(const nucleus::utils::MipmappedColourTexture& mipped_texture, unsigned array_index);
    void upload(const nucleus::Raster<glm::u8vec2>& texture, unsigned int array_index);
    void upload(const nucleus::Raster<uint16_t>& texture, unsigned int array_index);
    template <typename T> void upload(const nucleus::Raster<T>& texture);

    static GLenum compressed_texture_format();
    static nucleus::utils::ColourTexture::Format compression_algorithm();

protected:
    static GLenum max_anisotropy_param();
    static float max_anisotropy();

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

extern template void gl_engine::Texture::upload<uint16_t>(const nucleus::Raster<uint16_t>&);
extern template void gl_engine::Texture::upload<uint32_t>(const nucleus::Raster<uint32_t>&);
extern template void gl_engine::Texture::upload<glm::vec<2, uint32_t>>(const nucleus::Raster<glm::vec<2, uint32_t>>&);
extern template void gl_engine::Texture::upload<glm::vec<2, uint8_t>>(const nucleus::Raster<glm::vec<2, uint8_t>>&);
extern template void gl_engine::Texture::upload<glm::vec<4, uint8_t>>(const nucleus::Raster<glm::vec<4, uint8_t>>&);

} // namespace gl_engine

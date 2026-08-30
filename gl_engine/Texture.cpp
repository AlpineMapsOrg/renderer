/*****************************************************************************
 * AlpineMaps.org
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

#include "Texture.h"
#include "nucleus/utils/ColourTexture.h"

#include <QDebug>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QtAssert>
#include <algorithm>
#include <cmath>
#ifdef __EMSCRIPTEN__
#include <emscripten/html5_webgl.h>
#endif
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif

namespace {
struct GlParams {
    GLint internal_format = 0;
    GLint format = 0;
    GLint type = 0;
    unsigned n_elements = 0;
    unsigned n_bytes_per_element = 0;
    bool is_texture_filterable = false;
};

// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml
GlParams gl_tex_params(gl_engine::Texture::Format format)
{
    using F = gl_engine::Texture::Format;
    switch (format) {
    case F::CompressedRGBA8:
        return { GLint(gl_engine::Texture::compressed_texture_format()), 0, 0, 0, 0, true };
    case F::RGBA8:
        return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 1, true };
    case F::RGB565:
        return { GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 1, 2, true };
    case F::SRGBA8:
        return { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, 1, true };
    case F::RGBA8UI:
        return { GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 4, 1 };
    case F::RGBA32F:
        return { GL_RGBA32F, GL_RGBA, GL_FLOAT, 4, 4 };
    case F::RG8:
        return { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2, 1, true };
    case F::RG32UI:
        return { GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, 2, 4 };
    case F::RGB32UI:
        return { GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, 3, 4 };
    case F::R8UI:
        return { GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1, 1 };
    case F::R16UI:
        return { GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, 1, 2 };
    case F::R32UI:
        return { GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 1, 4 };
    case F::Invalid:
        return {};
    }
    return {};
}
} // namespace

gl_engine::Texture::Texture(Target target, Format format)
    : m_target(target)
    , m_format(format)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glGenTextures(1, &m_id);
}

gl_engine::Texture::~Texture()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glDeleteTextures(1, &m_id);
}

void gl_engine::Texture::bind(unsigned int texture_unit)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glActiveTexture(GL_TEXTURE0 + texture_unit);
    f->glBindTexture(GLenum(m_target), m_id);
}

void gl_engine::Texture::setParams(Filter min_filter, Filter mag_filter, bool anisotropic_filtering)
{
    // doesn't make sense, does it?
    Q_ASSERT(mag_filter != Filter::MipMapLinear);

    Q_ASSERT(gl_tex_params(m_format).is_texture_filterable || (min_filter == Filter::Nearest && mag_filter == Filter::Nearest));

    m_min_filter = min_filter;
    m_mag_filter = mag_filter;

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glTexParameteri(GLenum(m_target), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GLenum(m_target), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GLenum(m_target), GL_TEXTURE_MIN_FILTER, GLint(m_min_filter));
    f->glTexParameteri(GLenum(m_target), GL_TEXTURE_MAG_FILTER, GLint(m_mag_filter));
    if (anisotropic_filtering && max_anisotropy() > 0)
        f->glTexParameterf(GLenum(m_target), max_anisotropy_param(), max_anisotropy());
}

void gl_engine::Texture::allocate_array(unsigned int width, unsigned int height, unsigned int n_layers, unsigned mip_levels)
{
    Q_ASSERT(m_target == Target::_2dArray);
    Q_ASSERT(m_format != Format::Invalid);
    Q_ASSERT(width > 0);
    Q_ASSERT(height > 0);
    Q_ASSERT(n_layers > 0);

    const auto maximum_mip_levels = 1u + unsigned(std::floor(std::log2((std::max)(width, height))));
    Q_ASSERT(mip_levels <= maximum_mip_levels);

    auto mip_level_count = GLsizei(mip_levels);
    if (mip_level_count == 0) {
        mip_level_count = 1;
        if (m_min_filter == Filter::MipMapLinear)
            mip_level_count = GLsizei(1 + std::floor(std::log2(std::max(width, height))));
    }

    m_width = width;
    m_height = height;
    m_n_layers = n_layers;
    m_mip_levels = unsigned(mip_level_count);

    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glTexStorage3D(GLenum(m_target), mip_level_count, gl_tex_params(m_format).internal_format, GLsizei(width), GLsizei(height), GLsizei(n_layers));
}

void gl_engine::Texture::generate_mipmaps()
{
    Q_ASSERT(m_target == Target::_2dArray);
    Q_ASSERT(m_mip_levels > 1);
    Q_ASSERT(m_format != Format::CompressedRGBA8);
    Q_ASSERT(gl_tex_params(m_format).is_texture_filterable);
    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glGenerateMipmap(GLenum(m_target));
}

void gl_engine::Texture::upload(const nucleus::utils::ColourTexture& texture)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const auto width = GLsizei(texture.width());
    const auto height = GLsizei(texture.height());
    const auto p = gl_tex_params(m_format);
    if (m_format == Format::CompressedRGBA8) {
        Q_ASSERT(m_min_filter != Filter::MipMapLinear);
        const auto format = gl_engine::Texture::compressed_texture_format();
        f->glCompressedTexImage2D(GLenum(m_target), 0, format, width, height, 0, GLsizei(texture.n_bytes()), texture.data());
    } else if (m_format == Format::RGBA8 || m_format == Format::SRGBA8) {
        f->glTexImage2D(GLenum(m_target), 0, p.internal_format, width, height, 0, p.format, p.type, texture.data());
        if (m_min_filter == Filter::MipMapLinear)
            f->glGenerateMipmap(GLenum(m_target));
    } else {
        Q_ASSERT(false);
    }
}

void gl_engine::Texture::upload(const nucleus::utils::ColourTexture& texture, unsigned int array_index)
{
    Q_ASSERT(texture.width() == m_width);
    Q_ASSERT(texture.height() == m_height);
    Q_ASSERT(array_index < m_n_layers);
    Q_ASSERT(m_min_filter != Filter::MipMapLinear); // use the upload function with nucleus::utils::MipmappedColourTexture

    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const auto width = GLsizei(texture.width());
    const auto height = GLsizei(texture.height());
    if (m_format == Format::CompressedRGBA8) {
        const auto format = gl_engine::Texture::compressed_texture_format();
        f->glCompressedTexSubImage3D(GLenum(m_target), 0, 0, 0, GLint(array_index), width, height, 1, format, GLsizei(texture.n_bytes()), texture.data());
    } else if (m_format == Format::RGBA8 || m_format == Format::SRGBA8) {
        f->glTexSubImage3D(GLenum(m_target), 0, 0, 0, GLint(array_index), width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, texture.data());
    } else {
        Q_ASSERT(false);
    }
}

void gl_engine::Texture::upload(const nucleus::utils::MipmappedColourTexture& mipped_texture, unsigned int array_index)
{
    Q_ASSERT(mipped_texture.size() > 0);
    Q_ASSERT(mipped_texture.front().width() == m_width);
    Q_ASSERT(mipped_texture.front().height() == m_height);
    Q_ASSERT(array_index < m_n_layers);

    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    auto mip_level = 0;
    for (const auto& texture : mipped_texture) {
        const auto width = GLsizei(texture.width());
        const auto height = GLsizei(texture.height());
        if (m_format == Format::CompressedRGBA8) {
            const auto format = gl_engine::Texture::compressed_texture_format();
            f->glCompressedTexSubImage3D(
                GLenum(m_target), mip_level, 0, 0, GLint(array_index), width, height, 1, format, GLsizei(texture.n_bytes()), texture.data());
        } else if (m_format == Format::RGBA8 || m_format == Format::SRGBA8) {
            f->glTexSubImage3D(GLenum(m_target), mip_level, 0, 0, GLint(array_index), width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, texture.data());
        } else {
            Q_ASSERT(false);
        }
        ++mip_level;
    }
}

template <typename T> void gl_engine::Texture::upload(const radix::Raster<T>& texture, unsigned int array_index)
{
    Q_ASSERT(m_target == Target::_2dArray);

    const auto p = gl_tex_params(m_format);
    Q_ASSERT(m_format != Format::CompressedRGBA8);
    Q_ASSERT(m_format != Format::Invalid);
    Q_ASSERT(sizeof(T) == p.n_bytes_per_element * p.n_elements);
    if (!p.is_texture_filterable) {
        Q_ASSERT(m_mag_filter == Filter::Nearest);
        Q_ASSERT(m_min_filter == Filter::Nearest);
    }
    Q_ASSERT(array_index < m_n_layers);
    Q_ASSERT(texture.width() == m_width);
    Q_ASSERT(texture.height() == m_height);

    const auto width = GLsizei(texture.width());
    const auto height = GLsizei(texture.height());

    auto* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    f->glTexSubImage3D(GLenum(m_target), 0, 0, 0, GLint(array_index), width, height, 1, p.format, p.type, texture.bytes().data());

    if (m_min_filter == Filter::MipMapLinear)
        f->glGenerateMipmap(GLenum(m_target));
}
template void gl_engine::Texture::upload<uint8_t>(const radix::Raster<uint8_t>&, unsigned);
template void gl_engine::Texture::upload<uint16_t>(const radix::Raster<uint16_t>&, unsigned);
template void gl_engine::Texture::upload<uint32_t>(const radix::Raster<uint32_t>&, unsigned);
template void gl_engine::Texture::upload<glm::vec<2, uint8_t>>(const radix::Raster<glm::vec<2, uint8_t>>&, unsigned);
template void gl_engine::Texture::upload<glm::vec<2, uint32_t>>(const radix::Raster<glm::vec<2, uint32_t>>&, unsigned);
template void gl_engine::Texture::upload<glm::vec<3, uint32_t>>(const radix::Raster<glm::vec<3, uint32_t>>&, unsigned);
template void gl_engine::Texture::upload<glm::vec<4, uint8_t>>(const radix::Raster<glm::vec<4, uint8_t>>&, unsigned);
template void gl_engine::Texture::upload<glm::vec<4, float>>(const radix::Raster<glm::vec<4, float>>&, unsigned);

template <typename T> void gl_engine::Texture::upload(const radix::Raster<T>& texture)
{
    Q_ASSERT(m_target == Target::_2d);

    const auto p = gl_tex_params(m_format);
    Q_ASSERT(m_format != Format::CompressedRGBA8);
    Q_ASSERT(m_format != Format::Invalid);
    Q_ASSERT(sizeof(T) == p.n_bytes_per_element * p.n_elements);
    if (!p.is_texture_filterable) {
        Q_ASSERT(m_mag_filter == Filter::Nearest);
        Q_ASSERT(m_min_filter == Filter::Nearest);
    }

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    f->glTexImage2D(GLenum(m_target), 0, p.internal_format, GLsizei(texture.width()), GLsizei(texture.height()), 0, p.format, p.type, texture.bytes().data());

    if (m_min_filter == Filter::MipMapLinear)
        f->glGenerateMipmap(GLenum(m_target));
}
template void gl_engine::Texture::upload<uint8_t>(const radix::Raster<uint8_t>&);
template void gl_engine::Texture::upload<uint16_t>(const radix::Raster<uint16_t>&);
template void gl_engine::Texture::upload<uint32_t>(const radix::Raster<uint32_t>&);
template void gl_engine::Texture::upload<glm::vec<2, uint32_t>>(const radix::Raster<glm::vec<2, uint32_t>>&);
template void gl_engine::Texture::upload<glm::vec<3, uint32_t>>(const radix::Raster<glm::vec<3, uint32_t>>&);
template void gl_engine::Texture::upload<glm::vec<2, uint8_t>>(const radix::Raster<glm::vec<2, uint8_t>>&);
template void gl_engine::Texture::upload<glm::vec<4, uint8_t>>(const radix::Raster<glm::vec<4, uint8_t>>&);
template void gl_engine::Texture::upload<glm::vec<4, float>>(const radix::Raster<glm::vec<4, float>>&);

GLenum gl_engine::Texture::compressed_texture_format()
{
    // select between
    // DXT1, also called s3tc, old desktop compression
    // ETC1, old mobile compression
#if defined(__EMSCRIPTEN__)
    static const GLenum gl_texture_format = []() {
        const auto context = emscripten_webgl_get_current_context();
        if (!context)
            qFatal("No current WebGL context while detecting texture compression");
        if (emscripten_webgl_enable_extension(context, "WEBGL_compressed_texture_etc"))
            return GLenum(GL_COMPRESSED_SRGB8_ETC2);

        const bool s3tc = emscripten_webgl_enable_extension(context, "WEBGL_compressed_texture_s3tc");
        const bool s3tc_srgb = emscripten_webgl_enable_extension(context, "WEBGL_compressed_texture_s3tc_srgb");
        if (!s3tc || !s3tc_srgb)
            qFatal("Neither ETC nor sRGB S3TC texture compression is supported");
        return GLenum(GL_COMPRESSED_SRGB_S3TC_DXT1_EXT);
    }();
    return gl_texture_format;
#elif defined(__ANDROID__)
    return GL_COMPRESSED_SRGB8_ETC2;
#else
    return GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
#endif
}

nucleus::utils::ColourTexture::Format gl_engine::Texture::compression_algorithm()
{
#if defined(__EMSCRIPTEN__)
    static const auto compression_algorithm = []() {
        const auto context = emscripten_webgl_get_current_context();
        if (!context)
            qFatal("No current WebGL context while detecting texture compression");
        if (emscripten_webgl_enable_extension(context, "WEBGL_compressed_texture_etc"))
            return nucleus::utils::ColourTexture::Format::ETC1;

        const bool s3tc = emscripten_webgl_enable_extension(context, "WEBGL_compressed_texture_s3tc");
        const bool s3tc_srgb = emscripten_webgl_enable_extension(context, "WEBGL_compressed_texture_s3tc_srgb");
        if (!s3tc || !s3tc_srgb)
            qFatal("Neither ETC nor sRGB S3TC texture compression is supported");
        return nucleus::utils::ColourTexture::Format::DXT1;
    }();
    return compression_algorithm;
#elif defined(__ANDROID__)
    return nucleus::utils::ColourTexture::Format::ETC1;
#else
    return nucleus::utils::ColourTexture::Format::DXT1;
#endif
}

GLenum gl_engine::Texture::max_anisotropy_param()
{
#if defined(__EMSCRIPTEN__)
    static const GLenum param = []() {
        const auto context = emscripten_webgl_get_current_context();
        if (!context)
            qFatal("No current WebGL context while detecting anisotropic filtering");
        return emscripten_webgl_enable_extension(context, "EXT_texture_filter_anisotropic") ? GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT) : GLenum(0);
    }();
    return param;
#elif defined(__ANDROID__)
    return GL_TEXTURE_MAX_ANISOTROPY_EXT;
#else
    return GL_TEXTURE_MAX_ANISOTROPY;
#endif
}

float gl_engine::Texture::max_anisotropy()
{
#if defined(__EMSCRIPTEN__)
    static const float max_anisotropy = []() {
        const auto context = emscripten_webgl_get_current_context();
        if (!context)
            qFatal("No current WebGL context while detecting anisotropic filtering");
        if (!emscripten_webgl_enable_extension(context, "EXT_texture_filter_anisotropic"))
            return 0.f;

        GLfloat value = 0.f;
        QOpenGLContext::currentContext()->extraFunctions()->glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &value);
        return std::min(32.f, value);
    }();
    return max_anisotropy;
#elif defined(__ANDROID__)
    static const float max_anisotropy = []() {
        if (QOpenGLContext::currentContext()->hasExtension("GL_EXT_texture_filter_anisotropic")) {
            QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
            GLfloat t = 0.0f;
            f->glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &t);
            return std::min(32.f, t);
        }
        qDebug() << "GL_EXT_texture_filter_anisotropic not present";
        qDebug() << "present extensions: ";
        for (const auto& e : QOpenGLContext::currentContext()->extensions()) {
            qDebug() << e;
        }
        return 0.f;
    }();
    return max_anisotropy;
#else
    static const float max_anisotropy = []() {
        QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
        GLfloat t = 0.0f;
        f->glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &t);
        return std::min(32.f, t);
    }();
    return max_anisotropy;
#endif
}

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

#include "Texture.h"
#include "nucleus/utils/CompressedTexture.h"

#include <QOpenGLFunctions>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#endif
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif

gl_engine::Texture::Texture(Target target, Format format)
    : m_target(target)
    , m_format(format)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glGenTextures(1, &m_id);
    bind(0);
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

void gl_engine::Texture::setParams(Filter min_filter, Filter mag_filter)
{
    // doesn't make sense, does it?
    assert(mag_filter != Filter::MipMapLinear);

    // add upload functionality for compressed mipmaps to support this
    assert(m_format != Format::CompressedRGBA8 || min_filter != Filter::MipMapLinear);

    // webgl supports only nearest filtering for R16UI
    assert(m_format != Format::R16UI || (min_filter == Filter::Nearest && mag_filter == Filter::Nearest));

    m_min_filter = min_filter;
    m_mag_filter = mag_filter;

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLint(m_min_filter));
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLint(m_mag_filter));
}

void gl_engine::Texture::upload(const nucleus::utils::CompressedTexture& texture)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    switch (m_format) {
    case Format::CompressedRGBA8:
        f->glCompressedTexImage2D(GLenum(m_target), 0, gl_engine::Texture::compressed_texture_format(), GLsizei(texture.width()), GLsizei(texture.height()), 0,
            GLsizei(texture.n_bytes()), texture.data());
        return;
    case Format::RGBA8:
        f->glTexImage2D(GLenum(m_target), 0, GL_RGBA8, GLsizei(texture.width()), GLsizei(texture.height()), 0, GL_RGBA, GL_UNSIGNED_BYTE, texture.data());
        if (m_min_filter == Filter::MipMapLinear)
            f->glGenerateMipmap(GLenum(m_target));
        return;
    case Format::RG8:
    case Format::R16UI:
    case Format::Invalid:
        assert(false);
        break;
    }
    assert(false);
}

void gl_engine::Texture::upload(const nucleus::Raster<glm::u8vec2>& texture)
{
    assert(m_format == Format::RG8);

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    f->glTexImage2D(GLenum(m_target), 0, GL_RG8, GLsizei(texture.width()), GLsizei(texture.height()), 0, GL_RG, GL_UNSIGNED_BYTE, texture.bytes());

    if (m_min_filter == Filter::MipMapLinear)
        f->glGenerateMipmap(GLenum(m_target));
}

void gl_engine::Texture::upload(const nucleus::Raster<uint16_t>& texture)
{
    assert(m_format == Format::R16UI);

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GLenum(m_target), m_id);
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    f->glTexImage2D(GLenum(m_target), 0, GL_R16UI, GLsizei(texture.width()), GLsizei(texture.height()), 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, texture.bytes());

    // not Texture filterable according to https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml
    assert(m_min_filter != Filter::MipMapLinear);
}

GLenum gl_engine::Texture::compressed_texture_format()
{
    // select between
    // DXT1, also called s3tc, old desktop compression
    // ETC1, old mobile compression
#if defined(__EMSCRIPTEN__)
    // clang-format off
    static int gl_texture_format = EM_ASM_INT({
        var canvas = document.createElement('canvas');
        var gl = canvas.getContext("webgl2");
        const ext = gl.getExtension("WEBGL_compressed_texture_etc1");
        if (ext === null)
            return 0;
        return ext.COMPRESSED_RGB_ETC1_WEBGL;
    });
    qDebug() << "gl_texture_format from js: " << gl_texture_format;
    // clang-format on
    if (gl_texture_format == 0) {
        gl_texture_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; // not on mobile
    }
    return gl_texture_format;
#elif defined(__ANDROID__)
    return GL_COMPRESSED_RGB8_ETC2;
#else
    return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
#endif
}

nucleus::utils::CompressedTexture::Algorithm gl_engine::Texture::compression_algorithm()
{
#if defined(__EMSCRIPTEN__)
    // clang-format off
    static const int gl_texture_format = EM_ASM_INT({
        var canvas = document.createElement('canvas');
        var gl = canvas.getContext("webgl2");
        const ext = gl.getExtension("WEBGL_compressed_texture_etc1");
        if (ext === null)
            return 0;
        return ext.COMPRESSED_RGB_ETC1_WEBGL;
    });
    // clang-format on
    if (gl_texture_format == 0) {
        return nucleus::utils::CompressedTexture::Algorithm::DXT1;
    }
    return nucleus::utils::CompressedTexture::Algorithm::ETC1;
#elif defined(__ANDROID__)
    return nucleus::utils::CompressedTexture::Algorithm::ETC1;
#else
    return nucleus::utils::CompressedTexture::Algorithm::DXT1;
#endif
}

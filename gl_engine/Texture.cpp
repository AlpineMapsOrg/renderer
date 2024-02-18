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
#include "nucleus/utils/texture_compression.h"

#include <QOpenGLFunctions>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#endif
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif

gl_engine::Texture::Texture(Target target)
    : m_target(target)
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

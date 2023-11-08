 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "Framebuffer.h"

#include <iostream>

#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif



namespace gl_engine {



// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml
QOpenGLTexture::TextureFormat internal_format_qt(Framebuffer::ColourFormat f)
{
    switch (f) {
    case Framebuffer::ColourFormat::R8:
        return QOpenGLTexture::TextureFormat::R8_UNorm;
    case Framebuffer::ColourFormat::RGB8:
        return QOpenGLTexture::TextureFormat::RGB8_UNorm;
    case Framebuffer::ColourFormat::RGBA8:
        return QOpenGLTexture::TextureFormat::RGBA8_UNorm;
    case Framebuffer::ColourFormat::RG16UI:
        return QOpenGLTexture::TextureFormat::RG16U;
    case Framebuffer::ColourFormat::Float32:
        return QOpenGLTexture::TextureFormat::R32F;
    case Framebuffer::ColourFormat::RGB16F:
        return QOpenGLTexture::TextureFormat::RGB16F;
    case Framebuffer::ColourFormat::RGBA16F:
        return QOpenGLTexture::TextureFormat::RGBA16F;
    case Framebuffer::ColourFormat::R32UI:
        return QOpenGLTexture::TextureFormat::R32U;
    case Framebuffer::ColourFormat::RGBA32F:
        return QOpenGLTexture::TextureFormat::RGBA32F;
    }
    assert(false);
    return QOpenGLTexture::TextureFormat::NoFormat;
}

int format(Framebuffer::ColourFormat f)
{
    switch (f) {
    case Framebuffer::ColourFormat::R8:
        return GL_RED;
    case Framebuffer::ColourFormat::RGB8:
        return GL_RGB;
    case Framebuffer::ColourFormat::RGBA8:
        return GL_RGBA;
    case Framebuffer::ColourFormat::RG16UI:
        return QOpenGLTexture::PixelFormat::RG_Integer;
    case Framebuffer::ColourFormat::Float32: // reading Float32 is inefficient, see read_colour_attachment() for details.
        return GL_RED;
    case Framebuffer::ColourFormat::RGB16F:
        return GL_RGB;
    case Framebuffer::ColourFormat::RGBA16F:
        return GL_RGBA;
    case Framebuffer::ColourFormat::R32UI:
        return GL_RED_INTEGER;
    case Framebuffer::ColourFormat::RGBA32F:
        return GL_RGBA;
    }
    assert(false);
    return -1;
}

int format(Framebuffer::DepthFormat f)
{
    if (f != Framebuffer::DepthFormat::None) return GL_DEPTH_COMPONENT;
    return -1;
}

QOpenGLTexture::TextureFormat internal_format_qt(Framebuffer::DepthFormat f)
{
    switch (f) {
    case Framebuffer::DepthFormat::Int16:
        return QOpenGLTexture::TextureFormat::D16;
    case Framebuffer::DepthFormat::Int24:
        return QOpenGLTexture::TextureFormat::D24;
    case Framebuffer::DepthFormat::Float32:
        return QOpenGLTexture::TextureFormat::D32F;
    case Framebuffer::DepthFormat::None: // prevent compiler warning
        assert(false); // extra assert, so we can from the line number which issue it is
        return QOpenGLTexture::TextureFormat::NoFormat;
    }
    assert(false);
    return QOpenGLTexture::TextureFormat::NoFormat;
}

int type(Framebuffer::ColourFormat f)
{
    switch (f) {
    case Framebuffer::ColourFormat::R8: case Framebuffer::ColourFormat::RGBA8: case Framebuffer::ColourFormat::RGB8:
        return GL_UNSIGNED_BYTE;
    case Framebuffer::ColourFormat::RG16UI:
        return QOpenGLTexture::PixelType::UInt16;
    case Framebuffer::ColourFormat::Float32: case Framebuffer::ColourFormat::RGBA32F:
        return GL_FLOAT;
    case Framebuffer::ColourFormat::RGB16F: case Framebuffer::ColourFormat::RGBA16F:
        return GL_HALF_FLOAT;
    case Framebuffer::ColourFormat::R32UI:
        return GL_UNSIGNED_INT;
    }
    assert(false);
    return -1;
}

int type(Framebuffer::DepthFormat f)
{
    switch (f) {
    case Framebuffer::DepthFormat::Int16:
        return GL_UNSIGNED_SHORT;
    case Framebuffer::DepthFormat::Int24:
        return GL_UNSIGNED_INT;
    case Framebuffer::DepthFormat::Float32:
        return GL_FLOAT;
    case Framebuffer::DepthFormat::None: // prevent compiler warning
        assert(false); // extra assert, so we can from the line number which issue it is
        return -1;
    }
    assert(false);
    return -1;
}

// https://doc.qt.io/qt-6/qimage.html#Format-enum
QImage::Format qimage_format(Framebuffer::ColourFormat f)
{

    switch (f) {
    case Framebuffer::ColourFormat::R8:
        return QImage::Format_Grayscale8;
    case Framebuffer::ColourFormat::RGBA8:
        return QImage::Format_RGBA8888;
    case Framebuffer::ColourFormat::RGB8:
        return QImage::Format_RGB888;
    case Framebuffer::ColourFormat::RGB16F:
        return QImage::Format_RGB16;
    default:
         throw std::logic_error("unsupported, QImage does not support the color format of the texture");
    }

    assert(false);
    return QImage::Format_Invalid;
}


void Framebuffer::recreate_texture(int index) {
    if (index == -1) {
        if (m_depth_format != DepthFormat::None) {
            m_depth_texture->destroy();
            m_depth_texture->setFormat(internal_format_qt(m_depth_format));
            m_depth_texture->setSize(int(m_size.x), int(m_size.y));
            m_depth_texture->setAutoMipMapGenerationEnabled(false);
            // No filtering of depth buffer possible in OpenGLES and WebGL!!!!
            m_depth_texture->setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
            // No support on WebGL and OpenGL ES for Border (Warning: On those platforms we just ignore the wrap mode)
            // Feel free to make this code better!!
            m_depth_texture->setWrapMode(QOpenGLTexture::WrapMode::ClampToBorder);
#endif
            m_depth_texture->allocateStorage();
        }
    } else {
        m_colour_textures[index]->destroy();
        m_colour_textures[index]->setFormat(internal_format_qt(m_colour_definitions[index].format));
        m_colour_textures[index]->setSize(int(m_size.x), int(m_size.y));
        m_colour_textures[index]->setAutoMipMapGenerationEnabled(m_colour_definitions[index].autoMipMapGeneration);
        m_colour_textures[index]->setMinMagFilters(m_colour_definitions[index].minFilter, m_colour_definitions[index].magFilter);
#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
        // No support on WebGL and OpenGL ES for Border (Warning: On those platforms we just ignore the wrap mode)
        // Feel free to make this code better!!
        m_colour_textures[index]->setWrapMode(m_colour_definitions[index].wrapMode);
        m_colour_textures[index]->setBorderColor(m_colour_definitions[index].borderColor);
#endif
        // NOTE: If format and type not specifically defined in the following function it will crash for uint-textures
        // on OpenGL ES (Android). Might be a bug with the default of QOpenGLTexture on that platform.
        m_colour_textures[index]->allocateStorage((QOpenGLTexture::PixelFormat)format(m_colour_definitions[index].format), (QOpenGLTexture::PixelType)type(m_colour_definitions[index].format));
    }
}

void Framebuffer::recreate_all_textures() {
    recreate_texture(-1);
    for (int i = 0; i < m_colour_textures.size(); i++)
        recreate_texture(i);
}


Framebuffer::Framebuffer(DepthFormat depth_format, std::vector<TextureDefinition> colour_definitions, glm::uvec2 init_size)
    : m_depth_format(depth_format),
    m_colour_definitions(std::move(colour_definitions)),
    m_size(init_size)
{

    for (int i = 0; i < m_colour_definitions.size(); i++) {
        auto colorTexture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
        m_colour_textures.push_back(std::move(colorTexture));
    }
    if (m_depth_format != DepthFormat::None) {
        m_depth_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
    }

    recreate_all_textures();
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GL_TEXTURE_2D, 0);
    f->glGenFramebuffers(1, &m_frame_buffer);
    reset_fbo();
}

Framebuffer::~Framebuffer()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glDeleteFramebuffers(1, &m_frame_buffer);
}

void Framebuffer::resize(const glm::uvec2& new_size)
{
    m_size = new_size;
    recreate_all_textures();
    reset_fbo();
    unbind();
}

void Framebuffer::bind()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glViewport(0, 0, int(m_size.x), int(m_size.y));
    f->glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer);
    //reset_fbo();
}

void Framebuffer::bind_colour_texture(unsigned index, unsigned location)
{
    assert(index >= 0 && index < m_colour_textures.size());
    m_colour_textures[index]->bind(location);
}

void Framebuffer::bind_depth_texture(unsigned location)
{
    assert(m_depth_format != DepthFormat::None);
    m_depth_texture->bind(location);
}

std::unique_ptr<QOpenGLTexture> Framebuffer::take_and_replace_colour_attachment(unsigned index)
{
    std::unique_ptr<QOpenGLTexture> tmp = std::move(m_colour_textures[index]);
    m_colour_textures[index] = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
    recreate_texture(index);
    return std::move(tmp);
}

QImage Framebuffer::read_colour_attachment(unsigned index)
{
    assert(index >= 0 && index < m_colour_textures.size());

    auto texFormat = m_colour_definitions[index].format;

    // Float framebuffers can not be read efficiently on all environments.
    // that is, reading float red channel crashes on linux webassembly, but reading it as rgba is inefficient on linux native (4x slower, yes I measured).
    // i'm removing the old reading code altogether in order not to be tempted to use it in production (or by accident).
    // if you need to read a float32 buffer, pack the float into rgba8 values!
    if (texFormat != ColourFormat::RGBA8)
        qWarning() << "Reading back a different texture format than RGBA8 is not recommended.";

    bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glReadBuffer(GL_COLOR_ATTACHMENT0 + index);

    QImage image({ static_cast<int>(m_size.x), static_cast<int>(m_size.y) }, qimage_format(texFormat));
    assert(!image.isNull());
    f->glReadPixels(0, 0, int(m_size.x), int(m_size.y), format(texFormat), type(texFormat), image.bits());
    image.mirror();

    return image;
}

std::array<uchar, 4> Framebuffer::read_colour_attachment_pixel(unsigned index, const glm::dvec2& normalised_device_coordinates)
{
    assert(index >= 0 && index < m_colour_textures.size());

    auto texFormat = m_colour_definitions[index].format;
    assert(texFormat == ColourFormat::RGBA8);
    if (texFormat != ColourFormat::RGBA8)
        return {};

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    bind();
    f->glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
    std::array<uchar, 4> pixel;
    f->glReadPixels(
        int((normalised_device_coordinates.x + 1) / 2 * m_size.x),
        int((normalised_device_coordinates.y + 1) / 2 * m_size.y),
        1, 1, format(texFormat), type(texFormat), pixel.data());
    unbind();
    return pixel;
}

void Framebuffer::read_colour_attachment_pixel(unsigned index, const glm::dvec2& normalised_device_coordinates, void* target)
{
    assert(index >= 0 && index < m_colour_textures.size());

    auto texFormat = m_colour_definitions[index].format;

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    bind();
    f->glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
    f->glReadPixels(
        int((normalised_device_coordinates.x + 1) / 2 * m_size.x),
        int((normalised_device_coordinates.y + 1) / 2 * m_size.y),
        1, 1, format(texFormat), type(texFormat), target);
    unbind();
}

void Framebuffer::read_depth_attachment_pixel(const glm::dvec2& normalised_device_coordinates, void* target)
{
    auto texFormat = m_depth_format;

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    bind();
    f->glReadPixels(
        int((normalised_device_coordinates.x + 1) / 2 * m_size.x),
        int((normalised_device_coordinates.y + 1) / 2 * m_size.y),
        1, 1, format(texFormat), type(texFormat), target);
    unbind();
}

void Framebuffer::unbind()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::reset_fbo()
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    //QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer);
    unsigned int draw_attachments[m_colour_textures.size()];
    for (int i = 0; i < m_colour_textures.size(); i++) {
        draw_attachments[i] = GL_COLOR_ATTACHMENT0 + i;
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, draw_attachments[i], GL_TEXTURE_2D, m_colour_textures[i]->textureId(), 0);
    }
    if (m_depth_format != DepthFormat::None)
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth_texture->textureId(), 0);

    // Tell OpenGL how many attachments to use
    f->glDrawBuffers(m_colour_textures.size(), draw_attachments);

    assert(f->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}


glm::uvec2 Framebuffer::size() const
{
    return m_size;
}

}

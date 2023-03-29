/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

using gl_engine::Framebuffer;

namespace {

// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml
QOpenGLTexture::TextureFormat internal_format_qt(Framebuffer::ColourFormat f)
{
    switch (f) {
    case Framebuffer::ColourFormat::RGBA8:
        return QOpenGLTexture::TextureFormat::RGBA8_UNorm;
    case Framebuffer::ColourFormat::Float32:
        return QOpenGLTexture::TextureFormat::R32F;
    }
    assert(false);
    return QOpenGLTexture::TextureFormat::NoFormat;
}

int format(Framebuffer::ColourFormat f)
{
    switch (f) {
    case Framebuffer::ColourFormat::RGBA8:
        return GL_RGBA;
    case Framebuffer::ColourFormat::Float32:
        assert(false); // reading Float32 is inefficient, see read_colour_attachment() for details.
        break;
    }
    assert(false);
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
    case Framebuffer::ColourFormat::RGBA8:
        return GL_UNSIGNED_BYTE;
    case Framebuffer::ColourFormat::Float32:
        return GL_FLOAT;
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
    case Framebuffer::ColourFormat::RGBA8:
        return QImage::Format_RGBA8888;
    case Framebuffer::ColourFormat::Float32:
        throw std::logic_error("unsupported, QImage does not support float32");
    }
    assert(false);
    return QImage::Format_Invalid;
}
}


Framebuffer::Framebuffer(DepthFormat depth_format, std::vector<ColourFormat> colour_formats)
    : m_depth_format(depth_format),
    m_colour_formats(std::move(colour_formats)),
    m_size(4, 4)
{
    if (m_colour_formats.size() != 1)
        throw std::logic_error("not implemented");

    m_colour_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
    m_colour_texture->setFormat(internal_format_qt(m_colour_formats.front()));
    m_colour_texture->setSize(int(m_size.x), int(m_size.y));
    m_colour_texture->allocateStorage();

    if (m_depth_format != DepthFormat::None) {
        m_depth_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
        m_depth_texture->setFormat(internal_format_qt(m_depth_format));
        m_depth_texture->setSize(int(m_size.x), int(m_size.y));
        m_depth_texture->allocateStorage();
    }
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
    {
        m_colour_texture->destroy();
        m_colour_texture->setFormat(internal_format_qt(m_colour_formats.front()));
        m_colour_texture->setSize(int(m_size.x), int(m_size.y));
        m_colour_texture->setAutoMipMapGenerationEnabled(false);
        m_colour_texture->setMinMagFilters(QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
        m_colour_texture->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
        m_colour_texture->allocateStorage();
    }
    if (m_depth_format != DepthFormat::None) {
        m_depth_texture->destroy();
        m_depth_texture->setFormat(QOpenGLTexture::TextureFormat::D32F);
        m_depth_texture->setSize(int(m_size.x), int(m_size.y));
        m_colour_texture->setAutoMipMapGenerationEnabled(false);
        m_colour_texture->setMinMagFilters(QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
        m_colour_texture->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
        m_depth_texture->allocateStorage();
    }
    reset_fbo();
}

void Framebuffer::bind()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer);
    f->glViewport(0, 0, int(m_size.x), int(m_size.y));
}

void Framebuffer::bind_colour_texture(unsigned index)
{
    if (index != 0)
        throw std::logic_error("not implemented");

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GL_TEXTURE_2D, m_colour_texture->textureId());
}

std::unique_ptr<QOpenGLTexture> Framebuffer::take_and_replace_colour_attachment(unsigned index)
{
    if (index != 0)
        throw std::logic_error("not implemented");

    std::unique_ptr<QOpenGLTexture> tmp = std::move(m_colour_texture);
    m_colour_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
    resize(m_size);

    return tmp;
}

QImage Framebuffer::read_colour_attachment(unsigned index)
{
    if (index != 0)
        throw std::logic_error("not implemented");

    // Float framebuffers can not be read efficiently on all environments.
    // that is, reading float red channel crashes on linux webassembly, but reading it as rgba is inefficient on linux native (4x slower, yes I measured).
    // i'm removing the old reading code altogether in order not to be tempted to use it in production (or by accident).
    // if you need to read a float32 buffer, pack the float into rgba8 values!
    assert(m_colour_formats.front() == ColourFormat::RGBA8);
    if (m_colour_formats.front() != ColourFormat::RGBA8)
        return {};

    bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glReadBuffer(GL_COLOR_ATTACHMENT0);

    QImage image({ static_cast<int>(m_size.x), static_cast<int>(m_size.y) }, qimage_format(m_colour_formats.front()));
    assert(!image.isNull());
    f->glReadPixels(0, 0, int(m_size.x), int(m_size.y), format(m_colour_formats.front()), type(m_colour_formats.front()), image.bits());
    image.mirror();

    return image;
}

std::array<uchar, 4> Framebuffer::read_colour_attachment_pixel(unsigned index, const glm::dvec2& normalised_device_coordinates)
{
    if (index != 0)
        throw std::logic_error("not implemented");

    assert(m_colour_formats.front() == ColourFormat::RGBA8);
    if (m_colour_formats.front() != ColourFormat::RGBA8)
        return {};

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    bind();
    f->glReadBuffer(GL_COLOR_ATTACHMENT0);
    std::array<uchar, 4> pixel;
    f->glReadPixels(
        int((normalised_device_coordinates.x + 1) / 2 * m_size.x),
        int((normalised_device_coordinates.y + 1) / 2 * m_size.y),
        1, 1, format(m_colour_formats.front()), type(m_colour_formats.front()), pixel.data());
    unbind();
    return pixel;
}

void Framebuffer::unbind()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::reset_fbo()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer);
    f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colour_texture->textureId(), 0);

    if (m_depth_format != DepthFormat::None)
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth_texture->textureId(), 0);

    assert(f->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

namespace gl_engine {
glm::uvec2 Framebuffer::size() const
{
    return m_size;
}

}

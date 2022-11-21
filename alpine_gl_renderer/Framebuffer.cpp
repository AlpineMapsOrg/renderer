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

#include "Framebuffer.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>

namespace {

// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml
int internal_format(Framebuffer::ColourFormat f)
{
    switch (f) {
    case Framebuffer::ColourFormat::RGBA8:
        return GL_RGBA8;
        break;
    }
    assert(false);
    return -1;
}

int format(Framebuffer::ColourFormat f)
{
    switch (f) {
    case Framebuffer::ColourFormat::RGBA8:
        return GL_RGBA;
        break;
    }
    assert(false);
    return -1;
}

int internal_format(Framebuffer::DepthFormat f)
{
    switch (f) {
    case Framebuffer::DepthFormat::Int16:
        return GL_DEPTH_COMPONENT16;
    case Framebuffer::DepthFormat::Int24:
        return GL_DEPTH_COMPONENT24;
    case Framebuffer::DepthFormat::Float32:
        return GL_DEPTH_COMPONENT32F;
    case Framebuffer::DepthFormat::None: // prevent compiler warning
        assert(false); // extra assert, so we can from the line number which issue it is
        return -1;
    }
    assert(false);
    return -1;
}

int type(Framebuffer::ColourFormat f)
{
    switch (f) {
    case Framebuffer::ColourFormat::RGBA8:
        return GL_UNSIGNED_BYTE;
        break;
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

QImage::Format qimage_format(Framebuffer::ColourFormat f) {

    switch (f) {
    case Framebuffer::ColourFormat::RGBA8:
        return QImage::Format_RGBA8888;
        break;
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

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    {
        f->glGenTextures(1, &m_frame_buffer_colour);
        f->glBindTexture(GL_TEXTURE_2D, m_frame_buffer_colour);
        {
            const auto level = 0;
            const auto internalFormat = internal_format(m_colour_formats.front());
            const auto border = 0;
            const auto format = ::format(m_colour_formats.front());
            const auto type = ::type(m_colour_formats.front());
            const auto data = nullptr;
            f->glTexImage2D(GL_TEXTURE_2D, level, internalFormat, m_size.x, m_size.y, border, format, type, data);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
    if (m_depth_format != DepthFormat::None) {
        f->glGenTextures(1, &m_frame_buffer_depth);
        f->glBindTexture(GL_TEXTURE_2D, m_frame_buffer_depth);
        {
            const auto level = 0;
            const auto internalFormat = internal_format(m_depth_format);
            const auto border = 0;
            const auto format = GL_DEPTH_COMPONENT;
            const auto type = ::type(m_depth_format);
            const auto data = nullptr;
            f->glTexImage2D(GL_TEXTURE_2D, level, internalFormat, m_size.x, m_size.y, border, format, type, data);
        }
    }
    f->glBindTexture(GL_TEXTURE_2D, 0);
    {
        f->glGenFramebuffers(1, &m_frame_buffer);
        f->glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer);
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frame_buffer_colour, 0);

        if (m_depth_format != DepthFormat::None)
            f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_frame_buffer_depth, 0);
    }
}

void Framebuffer::resize(const glm::uvec2& new_size)
{
    m_size = new_size;
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    {
        f->glBindTexture(GL_TEXTURE_2D, m_frame_buffer_colour);
        const auto level = 0;
        const auto internalFormat = internal_format(m_colour_formats.front());
        const auto border = 0;
        const auto format = GL_RGBA;
        const auto type = ::type(m_colour_formats.front());
        const auto data = nullptr;
        f->glTexImage2D(GL_TEXTURE_2D, level, internalFormat, new_size.x, new_size.y, border, format, type, data);
    }
    if (m_depth_format != DepthFormat::None) {
        f->glBindTexture(GL_TEXTURE_2D, m_frame_buffer_depth);
        const auto level = 0;
        const auto internalFormat = internal_format(m_depth_format);
        const auto border = 0;
        const auto format = GL_DEPTH_COMPONENT;
        const auto type = ::type(m_depth_format);
        const auto data = nullptr;
        f->glTexImage2D(GL_TEXTURE_2D, level, internalFormat, new_size.x, new_size.y, border, format, type, data);
    }
}

void Framebuffer::bind()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer);
}

void Framebuffer::bind_colour_texture(unsigned index)
{
    if (index != 0)
        throw std::logic_error("not implemented");

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindTexture(GL_TEXTURE_2D, m_frame_buffer_colour);
}

QImage Framebuffer::read_colour_attachment(unsigned index)
{
    if (index != 0)
        throw std::logic_error("not implemented");

    QImage image({ static_cast<int>(m_size.x), static_cast<int>(m_size.y) }, qimage_format(m_colour_formats.front()));
    assert(!image.isNull());
    bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    f->glReadBuffer(GL_COLOR_ATTACHMENT0);
    f->glReadPixels(0, 0, m_size.x, m_size.y, format(m_colour_formats.front()), type(m_colour_formats.front()), image.bits());

    image.mirror();

    return image;
}

void Framebuffer::unbind()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


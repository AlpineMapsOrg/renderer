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

#include <QOpenGLFunctions>

Framebuffer::Framebuffer(DepthFormat depth_format, std::vector<ColourFormat> colour_formats)
    : m_depth_format(depth_format)
    , m_colour_formats(std::move(colour_formats))
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
            const auto format = GL_RGBA;
            const auto type = this->type(m_colour_formats.front());
            const auto data = nullptr;
            f->glTexImage2D(GL_TEXTURE_2D, level, internalFormat, 4, 4, border, format, type, data);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
    if (depth_format != DepthFormat::None) {
        f->glGenTextures(1, &m_frame_buffer_depth);
        f->glBindTexture(GL_TEXTURE_2D, m_frame_buffer_depth);
        {
            const auto level = 0;
            const auto internalFormat = internal_format(m_depth_format);
            const auto border = 0;
            const auto format = GL_DEPTH_COMPONENT;
            const auto type = this->type(m_depth_format);
            const auto data = nullptr;
            f->glTexImage2D(GL_TEXTURE_2D, level, internalFormat, 4, 4, border, format, type, data);
        }
    }
    f->glBindTexture(GL_TEXTURE_2D, 0);
    {
        f->glGenFramebuffers(1, &m_frame_buffer);
        f->glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer);
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frame_buffer_colour, 0);

        if (depth_format != DepthFormat::None)
            f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_frame_buffer_depth, 0);
    }
}

void Framebuffer::resize(const glm::uvec2& new_size)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    {
        f->glBindTexture(GL_TEXTURE_2D, m_frame_buffer_colour);
        const auto level = 0;
        const auto internalFormat = internal_format(m_colour_formats.front());
        const auto border = 0;
        const auto format = GL_RGBA;
        const auto type = GL_UNSIGNED_BYTE;
        const auto data = nullptr;
        f->glTexImage2D(GL_TEXTURE_2D, level, internalFormat, new_size.x, new_size.y, border, format, type, data);
    }
    {
        f->glBindTexture(GL_TEXTURE_2D, m_frame_buffer_depth);
        const auto level = 0;
        const auto internalFormat = internal_format(m_depth_format);
        const auto border = 0;
        const auto format = GL_DEPTH_COMPONENT;
        const auto type = this->type(m_depth_format);
        const auto data = nullptr;
        f->glTexImage2D(GL_TEXTURE_2D, level, internalFormat, new_size.x, new_size.y, border, format, type, data);
    }
}

void Framebuffer::bind()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer);
}

QImage Framebuffer::colour_attachment(unsigned index)
{
    return {};
}

void Framebuffer::unbind()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// https://registry.khronos.org/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml
int Framebuffer::internal_format(ColourFormat f) const
{
    switch (f) {
    case ColourFormat::RGBA8:
        return GL_RGB8;
        break;
    }
    assert(false);
    return -1;
}

int Framebuffer::internal_format(DepthFormat f) const
{
    switch (f) {
    case DepthFormat::Int16:
        return GL_DEPTH_COMPONENT16;
    case DepthFormat::Int24:
        return GL_DEPTH_COMPONENT24;
    case DepthFormat::Float32:
        return GL_DEPTH_COMPONENT32F;
    case DepthFormat::None: // prevent compiler warning
        assert(false);      // extra assert, so we can from the line number which issue it is
        return -1;
    }
    assert(false);
    return -1;
}

int Framebuffer::type(ColourFormat f) const
{
    switch (f) {
    case ColourFormat::RGBA8:
        return GL_UNSIGNED_BYTE;
        break;
    }
    assert(false);
    return -1;
}

int Framebuffer::type(DepthFormat f) const
{
    switch (f) {
    case DepthFormat::Int16:
        return GL_UNSIGNED_SHORT;
    case DepthFormat::Int24:
        return GL_UNSIGNED_INT;
    case DepthFormat::Float32:
        return GL_FLOAT;
    case DepthFormat::None: // prevent compiler warning
        assert(false);      // extra assert, so we can from the line number which issue it is
        return -1;
    }
    assert(false);
    return -1;
}

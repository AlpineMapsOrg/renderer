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
        return GL_RED;
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

    {
        m_colour_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
        m_colour_texture->setFormat(internal_format_qt(m_colour_formats.front()));
        m_colour_texture->setSize(int(m_size.x), int(m_size.y));
        m_colour_texture->allocateStorage();
    }
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

    bind();
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glReadBuffer(GL_COLOR_ATTACHMENT0);

    if (m_colour_formats.front() == ColourFormat::Float32) {
        std::vector<float> buffer;
        buffer.resize(size_t(m_size.x) * size_t(m_size.y));
        f->glReadPixels(0, 0, int(m_size.x), int(m_size.y), format(m_colour_formats.front()), type(m_colour_formats.front()), buffer.data());

        QImage image(static_cast<int>(m_size.x), static_cast<int>(m_size.y), QImage::Format_Grayscale8);
        const auto min_max = std::minmax_element(buffer.cbegin(), buffer.cend());
        const auto min = *min_max.first;
        const auto scale = *min_max.second - *min_max.first;
//        const auto min = 0;
//        const auto scale = 10;
        for (unsigned j = 0; j < m_size.y; ++j) {
            for (unsigned i = 0; i < m_size.x; ++i) {
                float fv = buffer[j * m_size.x + i];
                const auto v = uchar(std::min(255, std::max(0, int(255 * (fv - min) / scale))));
                // i don't understand why, but setting bits directly didn't work.
                image.setPixel(int(i), int(j), qRgb(v, v, v));
            }
        }
        image.mirror();
        return image;
    }

    QImage image({ static_cast<int>(m_size.x), static_cast<int>(m_size.y) }, qimage_format(m_colour_formats.front()));
    assert(!image.isNull());
    f->glReadPixels(0, 0, int(m_size.x), int(m_size.y), format(m_colour_formats.front()), type(m_colour_formats.front()), image.bits());
    image.mirror();

    return image;
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

/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "MapLabelManager.h"
#include "ShaderProgram.h"
#include "qopenglextrafunctions.h"

#include <QDirIterator>
#include <QFile>
#include <QThread>
#include <QTimer>

#include <iostream>
#include <string>

#include "nucleus/map_label/MapLabel.h"

namespace gl_engine {

MapLabelManager::MapLabelManager()
{
}
void MapLabelManager::init()
{
    m_mapLabelhandler.init();

    m_vao = std::make_unique<QOpenGLVertexArrayObject>();
    m_vao->create();
    m_vao->bind();

    m_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    m_index_buffer->create();
    m_index_buffer->bind();
    m_index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_index_buffer->allocate(m_mapLabelhandler.indices().data(), m_mapLabelhandler.indices().size() * sizeof(unsigned int));

    m_vertex_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    m_vertex_buffer->create();
    m_vertex_buffer->bind();
    m_vertex_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);

    std::vector<nucleus::MapLabel::VertexData> allLabels;
    for (const auto& label : m_mapLabelhandler.labels()) {
        allLabels.insert(allLabels.end(), label.vertices().begin(), label.vertices().end());
    }

    m_vertex_buffer->allocate(allLabels.data(), allLabels.size() * sizeof(nucleus::MapLabel::VertexData));
    m_instance_count = allLabels.size();

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    // vertex positions
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::MapLabel::VertexData), nullptr);
    f->glVertexAttribDivisor(0, 1); // buffer is active for 1 instance (for the whole quad)
    // uvs
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(nucleus::MapLabel::VertexData), (GLvoid*)(sizeof(glm::vec4)));
    f->glVertexAttribDivisor(1, 1); // buffer is active for 1 instance (for the whole quad)
    // world position
    f->glEnableVertexAttribArray(2);
    f->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(nucleus::MapLabel::VertexData), (GLvoid*)(sizeof(glm::vec4) * 2));
    f->glVertexAttribDivisor(2, 1); // buffer is active for 1 instance (for the whole quad)

    m_vao->release();

    // load the font texture
    font_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    font_texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    font_texture->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
    font_texture->create();

    font_texture->setSize(512, 512, 1);
    font_texture->setFormat(QOpenGLTexture::R8_UNorm);
    font_texture->allocateStorage();
    font_texture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, m_mapLabelhandler.font_bitmap());

    // load the icon texture
    QImage icon = m_mapLabelhandler.icon();
    icon_texture = std::make_unique<QOpenGLTexture>(icon);
    icon_texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    icon_texture->setMagnificationFilter(QOpenGLTexture::Linear);
    //    icon_texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    //    icon_texture->setWrapMode(QOpenGLTexture::WrapMode::MirroredRepeat);
    icon_texture->create();
}

void MapLabelManager::draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glEnable(GL_BLEND);

    glm::mat4 inv_view_rot = glm::inverse(camera.local_view_matrix());
    shader_program->set_uniform("inv_view_rot", inv_view_rot);

    font_texture->bind(3);
    shader_program->set_uniform("font_sampler", 3);

    icon_texture->bind(4);
    shader_program->set_uniform("icon_sampler", 4);

    m_vao->bind();

    f->glDrawElementsInstanced(GL_TRIANGLES, m_mapLabelhandler.indices().size(), GL_UNSIGNED_INT, 0, m_instance_count);

    m_vao->release();
}

} // namespace gl_engine

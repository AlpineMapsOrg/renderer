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

#include "MapLabel.h"

#include "nucleus/srs.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace gl_engine {

void MapLabel::init(QOpenGLExtraFunctions* f)
{
    m_label_position = nucleus::srs::lat_long_alt_to_world({ m_latitude, m_longitude, m_altitude });

    float size = 40.0; // size in pixel
    m_vertices = { -size / 2.0f, -size / 2.0f, 0, // bottom left
        -size / 2.0f, size / 2.0f, 0, // top left
        size / 2.0f, size / 2.0f, 0, // top right
        size / 2.0f, -size / 2.0f, 0 }; // bottom right

    m_indices = { 0, 2, 1,
        0, 3, 2 };

    m_vao = std::make_unique<QOpenGLVertexArrayObject>();
    m_vao->create();
    m_vao->bind();

    { // vao state
        m_vertex_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        m_vertex_buffer->create();
        m_vertex_buffer->bind();
        m_vertex_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);

        m_vertex_buffer->allocate(m_vertices.data(), m_vertices.size() * sizeof(GLfloat));
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT), nullptr);

        m_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
        m_index_buffer->create();
        m_index_buffer->bind();
        m_index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_index_buffer->allocate(m_indices.data(), m_indices.size() * sizeof(unsigned int));
    }
    m_vao->release();
}

void MapLabel::draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera, QOpenGLExtraFunctions* f) const
{
    m_vao->bind();

    float dist = glm::length(m_label_position - glm::vec3(camera.position()));
    float d = 1.0 / camera.to_screen_space(1, dist);
    glm::mat4 scale_matrix = glm::scale(glm::vec3(d));
    shader_program->set_uniform("scale_matrix", scale_matrix);

    shader_program->set_uniform("label_position", m_label_position);

    f->glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
    m_vao->release();
}

} // namespace gl_engine

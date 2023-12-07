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
#include <string>

namespace gl_engine {

void MapLabel::init(QOpenGLExtraFunctions* f, stbtt_bakedchar* character_data, int char_start, int char_end)
{
    int index_offset = 0;
    float offset_x = 0;
    float offset_y = -font_size / 2.0f + 75.0;

    float uv_width_norm = 1.0f / 512.0f;
    float uv_height_norm = 1.0f / 512.0f;

    bool special_char = false;
    const float text_spacing = 1.0f;

    std::string altitude_text = std::to_string(m_altitude);
    altitude_text = altitude_text.substr(0, altitude_text.find("."));

    std::string rendered_text = m_text + " (" + altitude_text + "m)";

    // get final Text Dimension
    float text_width = 0;
    for (const unsigned char& c : rendered_text) {
        if (int(c) == 195) {
            // we are parsing a special char -> next char in line shows us what exactly it is
            special_char = true;
            continue;
        }

        int char_index = int(c) - char_start;
        if (special_char) {
            char_index += 64;
            special_char = false;
        }

        const stbtt_bakedchar* b = character_data + char_index;
        float char_width = b->x1 - b->x0;

        text_width += char_width + text_spacing;
    }
    text_width -= text_spacing;

    offset_x -= text_width / 2.0;

    create_label_style(f, text_width, index_offset, offset_x, offset_y);

    for (const unsigned char& c : rendered_text) {
        if (int(c) == 195) {
            // we are parsing a special char -> next char in line shows us what exactly it is
            special_char = true;
            continue;
        }

        int char_index = int(c) - char_start;
        if (special_char) {
            char_index += 64;
            special_char = false;
        }

        const stbtt_bakedchar* b = character_data + char_index;
        float char_width = b->x1 - b->x0;
        float char_height = b->y0 - b->y1;

        m_vertices.push_back(offset_x - b->xoff);
        m_vertices.push_back(offset_y - b->yoff);

        m_vertices.push_back(offset_x - b->xoff);
        m_vertices.push_back(offset_y + char_height - b->yoff);

        m_vertices.push_back(offset_x + char_width - b->xoff);
        m_vertices.push_back(offset_y + char_height - b->yoff);

        m_vertices.push_back(offset_x + char_width - b->xoff);
        m_vertices.push_back(offset_y - b->yoff);

        m_uvs.push_back(b->x0 * uv_width_norm);
        m_uvs.push_back(b->y0 * uv_height_norm);

        m_uvs.push_back(b->x0 * uv_width_norm);
        m_uvs.push_back(b->y1 * uv_height_norm);

        m_uvs.push_back(b->x1 * uv_width_norm);
        m_uvs.push_back(b->y1 * uv_height_norm);

        m_uvs.push_back(b->x1 * uv_width_norm);
        m_uvs.push_back(b->y0 * uv_height_norm);

        m_indices.push_back(index_offset);
        m_indices.push_back(index_offset + 1);
        m_indices.push_back(index_offset + 2);

        m_indices.push_back(index_offset);
        m_indices.push_back(index_offset + 2);
        m_indices.push_back(index_offset + 3);

        index_offset += 4;
        offset_x += char_width + text_spacing;
    }

    m_label_position = nucleus::srs::lat_long_alt_to_world({ m_latitude, m_longitude, m_altitude });

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
        f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GL_FLOAT), nullptr);

        m_uv_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        m_uv_buffer->create();
        m_uv_buffer->bind();
        m_uv_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_uv_buffer->allocate(m_uvs.data(), m_uvs.size() * sizeof(GLfloat));
        f->glEnableVertexAttribArray(1);
        f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GL_FLOAT), nullptr);

        m_index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
        m_index_buffer->create();
        m_index_buffer->bind();
        m_index_buffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_index_buffer->allocate(m_indices.data(), m_indices.size() * sizeof(unsigned int));
    }
    m_vao->release();
}

void MapLabel::create_label_style(QOpenGLExtraFunctions* f, float text_width, int& index_offset, float offset_x, float offset_y)
{
    float padding_x = 10.0;
    float padding_y = 10.0;
    offset_x -= padding_x;
    offset_y -= padding_y + font_size / 4.0f;
    float width = text_width + 2.0 * padding_x;
    float height = font_size + 2.0 * padding_y;

    // box behind the label
    m_vertices.push_back(offset_x);
    m_vertices.push_back(offset_y);

    m_vertices.push_back(offset_x);
    m_vertices.push_back(offset_y + height);

    m_vertices.push_back(offset_x + width);
    m_vertices.push_back(offset_y + height);

    m_vertices.push_back(offset_x + width);
    m_vertices.push_back(offset_y);

    m_uvs.push_back(100);
    m_uvs.push_back(100);

    m_uvs.push_back(100);
    m_uvs.push_back(101);

    m_uvs.push_back(101);
    m_uvs.push_back(101);

    m_uvs.push_back(101);
    m_uvs.push_back(100);

    m_indices.push_back(index_offset);
    m_indices.push_back(index_offset + 2);
    m_indices.push_back(index_offset + 1);

    m_indices.push_back(index_offset);
    m_indices.push_back(index_offset + 3);
    m_indices.push_back(index_offset + 2);

    index_offset += 4;

    // icon below the label

    m_vertices.push_back(0);
    m_vertices.push_back(20.0);

    m_vertices.push_back(15.0);
    m_vertices.push_back(0);

    m_vertices.push_back(-15.0);
    m_vertices.push_back(0);

    m_uvs.push_back(100);
    m_uvs.push_back(100);

    m_uvs.push_back(100);
    m_uvs.push_back(100);

    m_uvs.push_back(100);
    m_uvs.push_back(100);

    m_indices.push_back(index_offset);
    m_indices.push_back(index_offset + 2);
    m_indices.push_back(index_offset + 1);

    index_offset += 3;
}

void MapLabel::draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera, QOpenGLExtraFunctions* f) const
{

    float dist = glm::length(m_label_position - glm::vec3(camera.position()));

    const float nearLabel = 100.0;
    const float farLabel = 500000.0;

    if (dist > nearLabel && dist < farLabel) {

        // uniform scaling regardless of distance between label and camera
        float uniform_scale = 1.0 / camera.to_screen_space(1, dist);

        // "soft" scaling -> farther labels are slightly smaller than nearer
        float dist_scale = 1.0 - ((dist - nearLabel) / (farLabel - nearLabel)) * 0.4f;
        dist_scale *= dist_scale;

        m_vao->bind();
        glm::mat4 scale_matrix = glm::scale(glm::vec3(uniform_scale * dist_scale));

        shader_program->set_uniform("scale_matrix", scale_matrix);
        shader_program->set_uniform("label_position", m_label_position);

        f->glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
        m_vao->release();
    }
}

} // namespace gl_engine

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

namespace nucleus {

void MapLabel::init(stbtt_bakedchar* character_data, int char_start, int char_end)
{
    float offset_x = 0;
    float offset_y = -font_size / 2.0f + 75.0;

    float uv_width_norm = 1.0f / 512.0f;
    float uv_height_norm = 1.0f / 512.0f;

    bool special_char = false;
    const float text_spacing = 1.0f;

    std::string altitude_text = std::to_string(m_altitude);
    altitude_text = altitude_text.substr(0, altitude_text.find("."));

    std::string rendered_text = m_text + " (" + altitude_text + "m)";

    m_label_position = nucleus::srs::lat_long_alt_to_world({ m_latitude, m_longitude, m_altitude });

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

    //    create_label_style(f, text_width, index_offset, offset_x, offset_y);

    // label icon
    m_vertices.push_back({ glm::vec4(-icon_size / 2.0, icon_size / 2.0, icon_size, -icon_size), // vertex position + offset
        glm::vec4(10, 10, 1, 1), // uv position + offset
        m_label_position });

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

        m_vertices.push_back({ glm::vec4(offset_x - b->xoff, offset_y - b->yoff, char_width, char_height), // vertex position + offset
            glm::vec4(b->x0 * uv_width_norm, b->y0 * uv_width_norm, (b->x1 - b->x0) * uv_width_norm, (b->y1 - b->y0) * uv_width_norm), // uv position + offset
            m_label_position });

        offset_x += char_width + text_spacing;
    }
}

const std::vector<MapLabel::VertexData>& MapLabel::vertices() const
{
    return m_vertices;
}

// void MapLabel::draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera, QOpenGLExtraFunctions* f) const
//{

//    // only draw labels that are not too near/far from camera
//    float dist = glm::length(m_label_position - glm::vec3(camera.position()));
//    const float nearLabel = 100.0;
//    const float farLabel = 500000.0;

//    if (dist > nearLabel && dist < farLabel) {
//        m_vao->bind();

//        f->glDrawElementsInstanced(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0, m_vertices.size());

//        m_vao->release();
//    }
//}

} // namespace nucleus

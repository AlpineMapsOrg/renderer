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

#pragma once

#include "ShaderProgram.h"
#include "qopenglextrafunctions.h"

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include "nucleus/camera/Definition.h"

#include "stb_truetype.h"

namespace gl_engine {

class MapLabel {
public:
    MapLabel(std::string text, double latitude, double longitude, float altitude, float importance)
        : m_text(text)
        , m_latitude(latitude)
        , m_longitude(longitude)
        , m_altitude(altitude)
        , m_importance(importance) {};

    void init(QOpenGLExtraFunctions* f, stbtt_bakedchar* character_data, int char_start, int char_end);
    void draw(ShaderProgram* shader_program, const nucleus::camera::Definition& camera, QOpenGLExtraFunctions* f) const;

    constexpr static float font_size = 30.0f;

private:
    void create_label_style(QOpenGLExtraFunctions* f, float text_width, int& index_offset, float offset_x, float offset_y);

    std::vector<unsigned int> m_indices;
    std::vector<GLfloat> m_vertices;
    std::vector<GLfloat> m_uvs;

    std::unique_ptr<QOpenGLBuffer> m_vertex_buffer;
    std::unique_ptr<QOpenGLBuffer> m_uv_buffer;
    std::unique_ptr<QOpenGLBuffer> m_index_buffer;
    std::unique_ptr<QOpenGLVertexArrayObject> m_vao;

    std::string m_text;
    double m_latitude;
    double m_longitude;
    float m_altitude;
    float m_importance;

    glm::vec3 m_label_position;
};
} // namespace gl_engine

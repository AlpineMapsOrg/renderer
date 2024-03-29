/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2024 Jakob Maier
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

#include <QObject>
#include <memory>
#include "nucleus/utils/GPX.h"
#include "nucleus/camera/Definition.h"
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>

#include "ShaderProgram.h"

class QOpenGLShaderProgram;

namespace gl_engine {

class ShaderProgram;

struct PolyLine {
    GLsizei point_count;
    std::unique_ptr<QOpenGLVertexArrayObject> vao = nullptr;
    std::unique_ptr<QOpenGLBuffer> vbo = nullptr;
};

class TrackManager : public QObject {
    Q_OBJECT
public:
    explicit TrackManager(QObject* parent = nullptr);

    void init();

    void draw(const nucleus::camera::Definition& camera, ShaderProgram* shader) const;

    void add_track(const nucleus::gpx::Gpx& gpx, ShaderProgram* shader);

    QOpenGLTexture* track_texture();

    unsigned int shading_method = 0U;
    float width = 7.0f;

private:
    size_t POINT_TEXTURE_SIZE = 10'000;
    float m_max_speed = 0.0f;
    float m_max_vertical_speed = 0.0f;
    size_t m_total_point_count = 0;
    std::vector<PolyLine> m_tracks;
    std::unique_ptr<QOpenGLTexture> m_data_texture = nullptr;
};
} // namespace gl_engine

/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2024 Jakob Maier
 * Copyright (C) 2024 Adam Celarek
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
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>

#include <nucleus/camera/Definition.h>
#include <nucleus/track/GPX.h>
#include <nucleus/track/Manager.h>

class QOpenGLShaderProgram;

namespace gl_engine {

class ShaderProgram;
class ShaderManager;

struct PolyLine {
    GLsizei point_count;
    std::unique_ptr<QOpenGLVertexArrayObject> vao = nullptr;
    std::unique_ptr<QOpenGLBuffer> vbo = nullptr;
    std::unique_ptr<QOpenGLTexture> texture = nullptr;
};

class TrackManager : public nucleus::track::Manager {
    Q_OBJECT
public:
    explicit TrackManager(ShaderManager* shader_manager, QObject* parent = nullptr);

    void draw(const nucleus::camera::Definition& camera) const;

public slots:
    void change_tracks(const QVector<nucleus::track::Gpx>& tracks) override;
    void change_display_width(float new_width) override;
    void change_shading_style(unsigned int new_style) override;

protected:
    void add_track(const nucleus::track::Gpx& gpx);

private:
    unsigned int m_shading_method = 0U;
    float m_display_width = 7.0f;
    ShaderProgram* m_shader;
    float m_max_speed = 0.0f;
    float m_max_vertical_speed = 0.0f;
    size_t m_total_point_count = 0;
    std::vector<PolyLine> m_tracks;
};
} // namespace gl_engine

/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include "TrackManager.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

#include "Polyline.h"
#include "ShaderProgram.h"

static const char* const debugVertexShaderSource = R"(
  layout(location = 0) in vec4 a_position;
  uniform highp mat4 matrix;
  void main() {
    gl_Position = matrix * a_position;
  })";

static const char* const debugFragmentShaderSource = R"(
  out lowp vec4 out_Color;
  void main() {
     out_Color = vec4(1.0, 0.0, 0.0, 1.0);
  })";

namespace gl_engine {

TrackManager::TrackManager(QObject* parent)
    : QObject(parent)
    , m_shader(std::make_unique<ShaderProgram>(debugVertexShaderSource, debugFragmentShaderSource))
{
}

void TrackManager::init()
{
    assert(QOpenGLContext::currentContext());
}

void TrackManager::draw(const nucleus::camera::Definition& camera) const
{
    m_shader->bind();
    m_shader->set_uniform("matrix", camera.local_view_projection_matrix(camera.position()));
    m_shader->set_uniform("camera_position", glm::vec3(camera.position()));

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

    for (const PolyLine& track : m_tracks) {
        track.vao->bind();
        f->glDrawArrays(GL_LINE_STRIP, 0U, track.vertex_count);
        track.vao->release();
    }

    m_shader->release();
}

void TrackManager::add_track(const nucleus::gpx::Gpx& gpx)
{
    auto points = nucleus::to_world_points(gpx);
    PolyLine polyline(points);
    m_tracks.push_back(std::move(polyline));
}
} // namespace gl_engine

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
#include "helpers.h"

#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

#include <iostream> // TODO: remove this

#include "Polyline.h"
#include "ShaderProgram.h"

namespace gl_engine
{

    TrackManager::TrackManager(QObject *parent)
        : QObject(parent), m_shader(std::make_unique<ShaderProgram>("polyline.vert", "polyline.frag"))
    {
    }

    void TrackManager::init()
    {
        assert(QOpenGLContext::currentContext());
    }

    void TrackManager::draw(const nucleus::camera::Definition &camera) const
    {
        QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

        f->glDisable(GL_CULL_FACE);

#if 0
        /* glLineWidth() is not guaranteed to be supported */
        float line_width = 3.0f;
        f->glLineWidth(line_width);
#endif

        auto matrix = camera.local_view_projection_matrix(camera.position());

        m_shader->bind();
        m_shader->set_uniform("matrix", matrix);
        m_shader->set_uniform("camera_position", glm::vec3(camera.position()));

        for (const PolyLine &track : m_tracks)
        {
            track.vao->bind();
            //f->glDrawArrays(GL_LINE_STRIP, 0, track.point_count);
            f->glDrawArrays(GL_TRIANGLES, 0, (track.point_count - 1) * 3);
        }

        m_shader->release();
        f->glEnable(GL_CULL_FACE);
    }

    void TrackManager::add_track(const nucleus::gpx::Gpx &gpx)
    {
        QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

        qDebug() << "TrackManager::add_track()\n";

        std::vector<glm::vec3> points = nucleus::to_world_points(gpx);

        std::vector<glm::vec3> ribbon = nucleus::to_world_ribbon(points);

        PolyLine polyline;

        polyline.vao = std::make_unique<QOpenGLVertexArrayObject>();
        polyline.vao->create();
        polyline.vao->bind();

        polyline.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        polyline.vbo->create();
        polyline.vbo->bind();
        polyline.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
#if 0
        polyline.vbo->allocate(points.data(), helpers::bufferLengthInBytes(points));
#else
        polyline.vbo->allocate(ribbon.data(), helpers::bufferLengthInBytes(ribbon));
#endif

        const auto position_attrib_location = m_shader->attribute_location("a_position");
        f->glEnableVertexAttribArray(position_attrib_location);
        f->glVertexAttribPointer(position_attrib_location, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        polyline.point_count = points.size();

        polyline.vao->release();

        m_tracks.push_back(std::move(polyline));
    }
} // namespace gl_engine
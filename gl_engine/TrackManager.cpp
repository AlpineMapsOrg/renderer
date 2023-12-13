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
#include <QOpenGLVersionFunctionsFactory>

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
#include <QOpenGLFunctions_3_3_Core>    // for wireframe mode
#endif

#include <iostream> // TODO: remove this

#include "Polyline.h"
#include "ShaderProgram.h"

#define USE_POINTS                      0
#define USE_RIBBON                      1
#define USE_RIBBON_WITH_NORMALS         2

#define RENDER_STRATEGY USE_RIBBON_WITH_NORMALS

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
        //QOpenGLFunctions *f2 = QOpenGLContext::currentContext()->functions();;
        auto funcs = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(QOpenGLContext::currentContext()); // for wireframe mode

        f->glDisable(GL_CULL_FACE);
#if 1
        funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

        auto matrix = camera.local_view_projection_matrix(camera.position());

        m_shader->bind();
        m_shader->set_uniform("matrix", matrix);
        m_shader->set_uniform("camera_position", glm::vec3(camera.position()));
        m_shader->set_uniform("width", 10.0f);

        for (const PolyLine &track : m_tracks)
        {
            track.vao->bind();

#if (RENDER_STRATEGY == USE_POINTS)
            f->glDrawArrays(GL_LINE_STRIP, 0, track.point_count);
#else
            f->glDrawArrays(GL_TRIANGLE_STRIP, 0, track.point_count * 2);
#endif
        }

        m_shader->release();
        f->glEnable(GL_CULL_FACE);
        funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    void TrackManager::add_track(const nucleus::gpx::Gpx &gpx)
    {
        QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

        qDebug() << "TrackManager::add_track()\n";

        std::vector<glm::vec3> points = nucleus::to_world_points(gpx);
        size_t point_count = points.size();

#if (RENDER_STRATEGY == USE_RIBBON)
        std::vector<glm::vec3> ribbon = nucleus::to_world_ribbon(points, 10.0f);
#elif (RENDER_STRATEGY == USE_RIBBON_WITH_NORMALS)
        std::vector<glm::vec3> ribbon = nucleus::to_world_ribbon_with_normals(points, 0.0f);
#endif

        PolyLine polyline;

        polyline.vao = std::make_unique<QOpenGLVertexArrayObject>();
        polyline.vao->create();
        polyline.vao->bind();

        polyline.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        polyline.vbo->create();
        polyline.vbo->bind();
        polyline.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);

#if (RENDER_STRATEGY == USE_POINTS)
        polyline.vbo->allocate(points.data(), helpers::bufferLengthInBytes(points));
#elif (RENDER_STRATEGY == USE_RIBBON || RENDER_STRATEGY == USE_RIBBON_WITH_NORMALS)
        polyline.vbo->allocate(ribbon.data(), helpers::bufferLengthInBytes(ribbon));
#else
#error Unknown Render Strategy
#endif

        const auto position_attrib_location = m_shader->attribute_location("a_position");
        f->glEnableVertexAttribArray(position_attrib_location);

#if (RENDER_STRATEGY == USE_POINTS || RENDER_STRATEGY == USE_RIBBON)
        f->glVertexAttribPointer(position_attrib_location, 3, GL_FLOAT, GL_FALSE, 1 * sizeof(glm::vec3), nullptr);
#endif

#if (RENDER_STRATEGY == USE_RIBBON_WITH_NORMALS)

        f->glVertexAttribPointer(position_attrib_location, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3), nullptr);

        const auto normal_attrib_location = m_shader->attribute_location("a_normal");
        f->glEnableVertexAttribArray(normal_attrib_location);
        f->glVertexAttribPointer(normal_attrib_location, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3), (void*)(sizeof(glm::vec3)));
#endif


        // glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec2), (void *)0);
        // glEnableVertexAttribArray(0);
        // glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec2), (void *)(sizeof(glm::vec2)));
        // glEnableVertexAttribArray(1);

        polyline.point_count = point_count;

        polyline.vao->release();

        m_tracks.push_back(std::move(polyline));
    }
} // namespace gl_engine

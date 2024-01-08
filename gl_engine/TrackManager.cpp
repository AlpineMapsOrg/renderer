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

#define WIREFRAME                       0
#define SMOOTH_POINTS                   1

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
        auto funcs = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(QOpenGLContext::currentContext()); // for wireframe mode

#if WIREFRAME
        funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

        auto matrix = camera.local_view_projection_matrix(camera.position());

        m_shader->bind();
        m_shader->set_uniform("matrix", matrix);
        m_shader->set_uniform("camera_position", glm::vec3(camera.position()));
        m_shader->set_uniform("width", width);
        m_shader->set_uniform("aspect", 16.0f / 9.0f); // TODO: make this dynamic
        m_shader->set_uniform("visualize_steepness", false); // TODO: make this dynamic
        m_shader->set_uniform("texin_vertices", 8);

        for (const PolyLine &track : m_tracks)
        {
            track.data_texture->bind(8);
            track.vao->bind();

#if (RENDER_STRATEGY == USE_POINTS)
            f->glDrawArrays(GL_LINE_STRIP, 0, track.point_count);
#else
            f->glDrawArrays(GL_TRIANGLE_STRIP, 0, track.point_count * 2 - 2);
#endif
        }

        m_shader->release();
        funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    void TrackManager::add_track(const nucleus::gpx::Gpx &gpx)
    {
        QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

        qDebug() << "TrackManager::add_track()\n";

        std::vector<glm::vec3> points = nucleus::to_world_points(gpx);

        // reduce variance in points
#if (SMOOTH_POINTS == 1)
        nucleus::gaussian_filter(points, 1.0f);
#endif

        size_t point_count = points.size();

#if (RENDER_STRATEGY == USE_RIBBON)
        std::vector<glm::vec3> ribbon = nucleus::to_world_ribbon(points, 10.0f);
#elif (RENDER_STRATEGY == USE_RIBBON_WITH_NORMALS)
        std::vector<glm::vec3> ribbon = nucleus::to_world_ribbon_with_normals(points, 0.0f);
#endif


        std::vector<glm::vec3> basic_ribbon = nucleus::to_world_ribbon(points, 0.0f);

        PolyLine polyline;

        polyline.data_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
        polyline.data_texture->setFormat(QOpenGLTexture::TextureFormat::RGB32F);
        polyline.data_texture->setSize(basic_ribbon.size(), 1);
        polyline.data_texture->setAutoMipMapGenerationEnabled(false);
        polyline.data_texture->setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
        polyline.data_texture->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
        polyline.data_texture->allocateStorage();
        polyline.data_texture->setData(QOpenGLTexture::RGB, QOpenGLTexture::Float32, basic_ribbon.data());

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

        f->glVertexAttribPointer(position_attrib_location, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(glm::vec3), nullptr);

        const auto normal_attrib_location = m_shader->attribute_location("a_tangent");
        f->glEnableVertexAttribArray(normal_attrib_location);
        f->glVertexAttribPointer(normal_attrib_location, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(glm::vec3), (void*)(sizeof(glm::vec3)));
#endif

#if 1
        const auto next_position_attrib_location = m_shader->attribute_location("a_next_position");
        assert(next_position_attrib_location <= 29);
        f->glEnableVertexAttribArray(next_position_attrib_location);
        f->glVertexAttribPointer(next_position_attrib_location, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(glm::vec3), (void*)(2 * sizeof(glm::vec3)));
#endif

        polyline.point_count = point_count;

        polyline.vao->release();

        m_tracks.push_back(std::move(polyline));
    }
} // namespace gl_engine

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

#include "TrackManager.h"

#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLVertexArrayObject>

#include "ShaderManager.h"
#include "ShaderProgram.h"
#include "helpers.h"

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
#include <QOpenGLFunctions_3_3_Core> // for wireframe mode
#endif

#define ENABLE_BOUNDING_QUADS 0

namespace gl_engine {

TrackManager::TrackManager(ShaderManager* shader_manager, QObject* parent)
    : nucleus::track::Manager(parent)
    , m_shader(shader_manager->track_program())
{
}

void TrackManager::draw(const nucleus::camera::Definition& camera) const
{
    if (m_tracks.empty()) {
        return;
    }

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();

#if (((defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)) && ENABLE_BOUNDING_QUADS)
    auto funcs = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(QOpenGLContext::currentContext()); // for wireframe mode
#endif

    f->glDisable(GL_CULL_FACE);

    auto view = camera.local_view_matrix();
    auto proj = camera.projection_matrix();

    m_shader->bind();
    m_shader->set_uniform("proj", proj);
    m_shader->set_uniform("view", view);
    m_shader->set_uniform("camera_position", glm::vec3(camera.position()));
    m_shader->set_uniform("width", m_display_width);
    m_shader->set_uniform("texin_track", 8);
    m_shader->set_uniform("shading_method", static_cast<int>(m_shading_method));
    m_shader->set_uniform("max_speed", m_max_speed);
    m_shader->set_uniform("max_vertical_speed", m_max_vertical_speed);
    m_shader->set_uniform("end_index", static_cast<int>(m_total_point_count));

    for (const PolyLine& track : m_tracks) {

        track.texture->bind(8);

        track.vao->bind();

        GLsizei vertex_count = (track.point_count - 1) * 6;

        m_shader->set_uniform("enable_intersection", true);
        f->glDrawArrays(GL_TRIANGLES, 0, vertex_count);

#if ENABLE_BOUNDING_QUADS
#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
        if (funcs) funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

        shader->set_uniform("enable_intersection", false);
        f->glDrawArrays(GL_TRIANGLES, 0, vertex_count);

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
        if (funcs) funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
#endif
    }

    m_shader->release();

    f->glEnable(GL_CULL_FACE);
}

void TrackManager::add_track(const nucleus::track::Gpx& gpx)
{
    using namespace nucleus::track;
    QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();

    qDebug() << "Segment Count: " << gpx.track.size();

    for (const Segment& segment : gpx.track) {

        qDebug() << "Point Count Per Segment: " << segment.size();

        // transform from latitude and longitude into renderer world coordinates
        std::vector<glm::vec4> points = to_world_points(segment);

        // data cleanup
        apply_gaussian_filter(points, 1.0f);

        for (size_t i = 0; i < points.size() - 1; ++i) {
            glm::vec4 a = points[i];
            glm::vec4 b = points[i + 1];
            float distance = glm::distance(glm::vec3(a), glm::vec3(b));
            float time = b.w;
            float speed = distance / time;
            float vertical_speed = glm::abs(a.z - b.z) / time;

            m_max_speed = glm::max(speed, m_max_speed);
            m_max_vertical_speed = glm::max(vertical_speed, m_max_vertical_speed);
        }

        size_t point_count = points.size();

        std::vector<glm::vec3> basic_ribbon = triangles_ribbon(points, 0.0f, 0);

        PolyLine polyline = {};



        if (polyline.texture == nullptr) {

            int max_texture_size;
            f->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

            qDebug() << "max texture size: " << max_texture_size;

            if (max_texture_size < int(point_count)) {
                qDebug() << "Unable to add track with " << point_count << "points, maximum is " << max_texture_size;
                return;
            }

            // create texture to hold the point data
            polyline.texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
            polyline.texture->setFormat(QOpenGLTexture::TextureFormat::RGBA32F);
            polyline.texture->setSize(point_count, 1);
            polyline.texture->setAutoMipMapGenerationEnabled(false);
            polyline.texture->setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
            polyline.texture->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
            polyline.texture->allocateStorage();

            if (!polyline.texture->isStorageAllocated()) {
                qDebug() << "Could not allocate texture storage!";
                return;
            }
        }

        polyline.texture->bind();
        polyline.texture->setData(0, 0, 0, point_count, 1, 0, QOpenGLTexture::RGBA, QOpenGLTexture::Float32, points.data());

        m_total_point_count += point_count;

        polyline.vao = std::make_unique<QOpenGLVertexArrayObject>();
        polyline.point_count = point_count;
        polyline.vao->create();
        polyline.vao->bind();

        polyline.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        polyline.vbo->create();

        polyline.vbo->bind();
        polyline.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);

        polyline.vbo->allocate(basic_ribbon.data(), helpers::bufferLengthInBytes(basic_ribbon));

        GLsizei stride = 3 * sizeof(glm::vec3);

        const int position_attrib_location = m_shader->attribute_location("a_position");
        f->glEnableVertexAttribArray(position_attrib_location);
        f->glVertexAttribPointer(position_attrib_location, 3, GL_FLOAT, GL_FALSE, stride, nullptr);

        const int direction_attrib_location = m_shader->attribute_location("a_direction");
        f->glEnableVertexAttribArray(direction_attrib_location);
        f->glVertexAttribPointer(direction_attrib_location, 3, GL_FLOAT, GL_FALSE, stride, (void*)(1 * sizeof(glm::vec3)));

        const int offset_attrib_location = m_shader->attribute_location("a_offset");
        f->glEnableVertexAttribArray(offset_attrib_location);
        f->glVertexAttribPointer(offset_attrib_location, 3, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(glm::vec3)));

        polyline.vao->release();

        m_tracks.push_back(std::move(polyline));
    }

    qDebug() << "Total Point Count: " << m_total_point_count;
}

void TrackManager::change_tracks(const QVector<nucleus::track::Gpx>& tracks)
{
    m_tracks.resize(0);
    for (const auto& t : tracks) {
        add_track(t);
    }
}

void TrackManager::change_display_width(float new_width) { m_display_width = new_width; }

void TrackManager::change_shading_style(unsigned int new_style) { m_shading_method = new_style; }
} // namespace gl_engine

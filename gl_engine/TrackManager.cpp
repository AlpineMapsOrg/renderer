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
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLVertexArrayObject>

#if (defined(__linux) && !defined(__ANDROID__)) || defined(_WIN32) || defined(_WIN64)
#include <QOpenGLFunctions_3_3_Core> // for wireframe mode
#endif

#include <iostream> // TODO: remove this

#include "Polyline.h"
#include "ShaderProgram.h"

#define WIREFRAME 0

namespace gl_engine {

TrackManager::TrackManager(QObject* parent)
    : QObject(parent)
{
}

void TrackManager::init() { assert(QOpenGLContext::currentContext()); }

QOpenGLTexture* TrackManager::track_texture()
{
    if (m_tracks.size() == 0) {
        return nullptr;
    }
    return m_tracks[0].data_texture.get();
}

void TrackManager::draw(const nucleus::camera::Definition& camera, ShaderProgram* shader) const
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    auto funcs = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(QOpenGLContext::currentContext()); // for wireframe mode

#if WIREFRAME
    funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif


    funcs->glDisable(GL_CULL_FACE);

    auto matrix = camera.local_view_projection_matrix(camera.position());

    shader->bind();
    shader->set_uniform("matrix", matrix);
    shader->set_uniform("camera_position", glm::vec3(camera.position()));
    shader->set_uniform("width", width);
    shader->set_uniform("aspect", 16.0f / 9.0f); // TODO: make this dynamic
    shader->set_uniform("visualize_steepness", false); // TODO: make this dynamic
    shader->set_uniform("texin_track", 8);

    for (const PolyLine& track : m_tracks) {
        track.data_texture->bind(8);
        track.vao->bind();

        //GLsizei count = (track.point_count - 1) * 6;
        GLsizei triangle_count = (track.point_count - 1) * 2;

        funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        shader->set_uniform("enable_intersection", true);
        f->glDrawArrays(GL_TRIANGLES, 0, triangle_count);

        funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        shader->set_uniform("enable_intersection", false);
        f->glDrawArrays(GL_TRIANGLES, 0, triangle_count);
    }

    shader->release();
    funcs->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    funcs->glEnable(GL_CULL_FACE);
}

void TrackManager::add_track(const nucleus::gpx::Gpx& gpx, ShaderProgram* shader)
{
    QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();
    (void)shader;


    // transform from latitude and longitude into renderer world
    // coordinates
    std::vector<glm::vec3> points = nucleus::to_world_points(gpx);

    // reduce variance in points
    nucleus::apply_gaussian_filter(points, 1.0f);

    size_t point_count = points.size();

#if 0
    std::vector<glm::vec3> basic_ribbon = nucleus::triangle_strip_ribbon(points, 15.0f);
#else
    std::vector<glm::vec3> basic_ribbon = nucleus::triangles_ribbon(points, 0.0f);
#endif

    PolyLine polyline;

    // create texture to hold the vertex data
    polyline.data_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target::Target2D);
    polyline.data_texture->setFormat(QOpenGLTexture::TextureFormat::RGB32F);
    polyline.data_texture->setSize(points.size(), 1);
    polyline.data_texture->setAutoMipMapGenerationEnabled(false);
    polyline.data_texture->setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
    polyline.data_texture->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
    polyline.data_texture->allocateStorage();
    polyline.data_texture->setData(QOpenGLTexture::RGB, QOpenGLTexture::Float32, points.data());

    // even if the vao is not used, we still need a dummy vao
    polyline.vao = std::make_unique<QOpenGLVertexArrayObject>();
    polyline.vao->create();
    polyline.vao->bind();

#if 1
    polyline.vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    polyline.vbo->create();

    polyline.vbo->bind();
    polyline.vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);

    polyline.vbo->allocate(basic_ribbon.data(), helpers::bufferLengthInBytes(basic_ribbon));

    const auto position_attrib_location = shader->attribute_location("a_position");
    f->glEnableVertexAttribArray(position_attrib_location);


    f->glVertexAttribPointer(position_attrib_location, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3), nullptr);

    const auto normal_attrib_location = shader->attribute_location("a_offset");
    f->glEnableVertexAttribArray(normal_attrib_location);
    f->glVertexAttribPointer(normal_attrib_location, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3), (void*)(sizeof(glm::vec3)));

    //const auto next_position_attrib_location = shader->attribute_location("a_next_position");
    //f->glEnableVertexAttribArray(next_position_attrib_location);
    //f->glVertexAttribPointer(next_position_attrib_location, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(glm::vec3), (void*)(2 * sizeof(glm::vec3)));

#if 0
    auto indices = nucleus::ribbon_indices(points.size());
    polyline.ebo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    polyline.ebo->create();
    polyline.ebo->bind();
    polyline.ebo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    polyline.ebo->allocate(indices.data(), helpers::bufferLengthInBytes(indices));
#endif

#endif
    polyline.point_count = point_count;

    polyline.vao->release();

    m_tracks.push_back(std::move(polyline));
}
} // namespace gl_engine

 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include <vector>
#include <memory>

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLVertexArrayObject>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace gl_engine::helpers {
inline QMatrix4x4 toQtType(const glm::mat4& mat)
{
    return QMatrix4x4(glm::value_ptr(mat)).transposed();
}
inline QVector4D toQtType(const glm::vec4& v)
{
    return QVector4D(v.x, v.y, v.z, v.w);
}
inline QVector3D toQtType(const glm::vec3& v)
{
    return QVector3D(v.x, v.y, v.z);
}
inline QVector2D toQtType(const glm::vec2& v)
{
    return QVector2D(v.x, v.y);
}
template <typename T>
T toQtType(const T& v)
{
    return v;
}


template <typename T>
inline int bufferLengthInBytes(const std::vector<T>& vec)
{
    return int(vec.size() * sizeof(T));
}


struct ScreenQuadGeometry {
    std::unique_ptr<QOpenGLVertexArrayObject> vao;
    std::unique_ptr<QOpenGLBuffer> index_buffer;
    inline void draw() const {
        QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
        vao->bind();
        f->glDisable(GL_DEPTH_TEST);
        f->glDrawElements(GL_TRIANGLE_STRIP, 3, GL_UNSIGNED_SHORT, nullptr);
        vao->release();
    }
    inline void draw_with_depth_test() const {
        QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
        vao->bind();
        f->glEnable(GL_DEPTH_TEST);
        f->glDrawElements(GL_TRIANGLE_STRIP, 3, GL_UNSIGNED_SHORT, nullptr);
        vao->release();
    }
};

inline ScreenQuadGeometry create_screen_quad_geometry()
{
    ScreenQuadGeometry geometry;
    geometry.vao = std::make_unique<QOpenGLVertexArrayObject>();
    geometry.vao->create();
    geometry.vao->bind();
    { // vao state
        const std::array<unsigned short, 3> indices = { 0, 1, 2 };
        geometry.index_buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
        geometry.index_buffer->create();
        geometry.index_buffer->bind();
        geometry.index_buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
        geometry.index_buffer->allocate(indices.data(), 3 * sizeof(unsigned short));
    }
    geometry.vao->release();
    return geometry;
}

}

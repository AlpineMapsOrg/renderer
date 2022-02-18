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

#include "GLDebugPainter.h"

#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

#include "alpine_gl_renderer/GLHelpers.h"

GLDebugPainter::GLDebugPainter(QObject *parent)
  : QObject{parent}
{

}

void GLDebugPainter::setAttributeLocations(const DebugGLAttributeLocations& d)
{
  m_attribute_locations = d;
}

void GLDebugPainter::setUniformLocations(const DebugGLUniformLocations& d)
{
  m_uniform_locations = d;
}

void GLDebugPainter::activate(QOpenGLShaderProgram* shader_program, const glm::mat4& world_view_projection_matrix)
{
  shader_program->setUniformValue(m_uniform_locations.view_projection_matrix, gl_helpers::toQtType(world_view_projection_matrix));
}

void GLDebugPainter::drawLineStrip(const std::vector<glm::vec3>& points) const
{
  QOpenGLVertexArrayObject vao;
  vao.create();
  vao.bind();

  QOpenGLBuffer buffer(QOpenGLBuffer::VertexBuffer);
  buffer.create();
  buffer.bind();
  buffer.setUsagePattern(QOpenGLBuffer::StreamDraw);
  buffer.allocate(points.data(), gl_helpers::bufferLengthInBytes(points));

  QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();
  f->glEnableVertexAttribArray(GLuint(m_attribute_locations.position));
//  f->glVertexAttribPointer(GLuint(m_attribute_locations.position), /*size*/ 3, /*type*/ GL_FLOAT, /*normalised*/ GL_FALSE, /*stride*/ 0, nullptr);
//  f->glDrawArrays(GL_LINE_STRIP, 0, points.size());

  vao.release();
}

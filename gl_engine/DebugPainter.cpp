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

#include "DebugPainter.h"

#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

#include "ShaderProgram.h"
#include "helpers.h"

gl_engine::DebugPainter::DebugPainter(QObject* parent)
    : QObject { parent }
{
}

void gl_engine::DebugPainter::activate(ShaderProgram* shader_program, const glm::mat4& world_view_projection_matrix)
{
    shader_program->set_uniform("matrix", world_view_projection_matrix);
}

void gl_engine::DebugPainter::draw_line_strip(ShaderProgram* shader_program, const std::vector<glm::vec3>& points) const
{
    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    QOpenGLBuffer buffer(QOpenGLBuffer::VertexBuffer);
    buffer.create();
    buffer.bind();
    buffer.setUsagePattern(QOpenGLBuffer::StreamDraw);
    buffer.allocate(points.data(), gl_engine::helpers::bufferLengthInBytes(points));

    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    const auto position_attrib_location = shader_program->attribute_location("a_position");
    f->glEnableVertexAttribArray(position_attrib_location);
    f->glVertexAttribPointer(position_attrib_location, /*size*/ 3, /*type*/ GL_FLOAT, /*normalised*/ GL_FALSE, /*stride*/ 0, nullptr);
    f->glDrawArrays(GL_LINE_STRIP, 0, int(points.size()));

    vao.release();
}

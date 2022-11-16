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

#include "GLShaderManager.h"

#include <QOpenGLContext>

static const char* const tileVertexShaderSource = R"(
  layout(location = 0) in highp float height;
  out lowp vec2 uv;
  uniform highp mat4 matrix;
  uniform highp vec3 camera_position;
  uniform highp vec4 bounds[32];
  uniform int n_edge_vertices;
  void main() {
    int row = gl_VertexID / n_edge_vertices;
    int col = gl_VertexID - (row * n_edge_vertices);
    int geometry_id = 0;
    float tile_width = (bounds[geometry_id].z - bounds[geometry_id].x) / float(n_edge_vertices - 1);
    float tile_height = (bounds[geometry_id].w - bounds[geometry_id].y) / float(n_edge_vertices - 1);

    vec4 pos = vec4(float(col) * tile_width + bounds[geometry_id].x,
                   float(n_edge_vertices - row - 1) * tile_width + bounds[geometry_id].y,
                   height * 65536.0 * 0.125 - camera_position.z,
                   1.0);
    uv = vec2(float(col) / float(n_edge_vertices - 1), float(row) / float(n_edge_vertices - 1));
    gl_Position = matrix * pos;
  })";

static const char* const tileFragmentShaderSource = R"(
  in lowp vec2 uv;
  uniform sampler2D texture_sampler;
  out lowp vec4 out_Color;
  void main() {
     out_Color = texture(texture_sampler, uv);
     //gl_FragDepth = gl_FragCoord.z;
  })";

static const char* const screenQuadVertexShaderSource = R"(
// https://stackoverflow.com/a/59739538
out highp vec2 texcoords; // texcoords are in the normalized [0,1] range for the viewport-filling quad part of the triangle
void main() {
    vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0),
                             vec2(3.0, -1.0),
                             vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
})";

static const char* const screenQuadFragmentShaderSource = R"(
  in highp vec2 texcoords;
  uniform sampler2D texture_sampler;
  out lowp vec4 out_Color;
  void main() {
     out_Color = vec4(texcoords.xy, 0.0, 1.0) * 0.1 + texture(texture_sampler, texcoords);
  })";

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

QByteArray versionedShaderCode(const char* const src)
{
    QByteArray versionedSrc;

    if (QOpenGLContext::currentContext()->isOpenGLES())
        versionedSrc.append(QByteArrayLiteral("#version 300 es\n"));
    else
        versionedSrc.append(QByteArrayLiteral("#version 330\n"));

    versionedSrc.append(src);
    return versionedSrc;
}

GLShaderManager::GLShaderManager()
{
    m_tile_program = std::make_unique<QOpenGLShaderProgram>();
    m_tile_program->addShaderFromSourceCode(QOpenGLShader::Vertex, versionedShaderCode(tileVertexShaderSource));
    m_tile_program->addShaderFromSourceCode(QOpenGLShader::Fragment, versionedShaderCode(tileFragmentShaderSource));
    {
        const auto link_success = m_tile_program->link();
        assert(link_success);
    }
    m_debug_program = std::make_unique<QOpenGLShaderProgram>();
    m_debug_program->addShaderFromSourceCode(QOpenGLShader::Vertex, versionedShaderCode(debugVertexShaderSource));
    m_debug_program->addShaderFromSourceCode(QOpenGLShader::Fragment, versionedShaderCode(debugFragmentShaderSource));
    {
        const auto link_success = m_debug_program->link();
        assert(link_success);
    }
    m_screen_quad_program = std::make_unique<QOpenGLShaderProgram>();
    m_screen_quad_program->addShaderFromSourceCode(QOpenGLShader::Vertex, versionedShaderCode(screenQuadVertexShaderSource));
    m_screen_quad_program->addShaderFromSourceCode(QOpenGLShader::Fragment, versionedShaderCode(screenQuadFragmentShaderSource));
    {
        const auto link_success = m_screen_quad_program->link();
        assert(link_success);
    }

    // tile shader
    m_tile_uniform_location.view_projection_matrix = m_tile_program->uniformLocation("matrix");
    assert(m_tile_uniform_location.view_projection_matrix != -1);

    m_tile_uniform_location.camera_position = m_tile_program->uniformLocation("camera_position");
    assert(m_tile_uniform_location.camera_position != -1);

    m_tile_uniform_location.bounds_array = m_tile_program->uniformLocation("bounds");
    assert(m_tile_uniform_location.bounds_array != -1);

    m_tile_uniform_location.n_edge_vertices = m_tile_program->uniformLocation("n_edge_vertices");
    assert(m_tile_uniform_location.n_edge_vertices != -1);

    m_tile_attribute_locations.height = m_tile_program->attributeLocation("height");
    assert(m_tile_attribute_locations.height != -1);

    // debug shader
    m_debug_uniform_location.view_projection_matrix = m_debug_program->uniformLocation("matrix");
    assert(m_tile_uniform_location.view_projection_matrix != -1);

    m_debug_attribute_locations.position = 0;
}

GLShaderManager::~GLShaderManager() = default;

void GLShaderManager::bindTileShader()
{
    m_tile_program->bind();
}

void GLShaderManager::bindDebugShader()
{
    m_debug_program->bind();
}

QOpenGLShaderProgram* GLShaderManager::tileShader() const
{
    return m_tile_program.get();
}

QOpenGLShaderProgram* GLShaderManager::debugShader() const
{
    return m_debug_program.get();
}

QOpenGLShaderProgram* GLShaderManager::screen_quad_program() const
{
    return m_screen_quad_program.get();
}


void GLShaderManager::release()
{
    m_tile_program->release();
}

TileGLAttributeLocations GLShaderManager::tileAttributeLocations() const
{
    return m_tile_attribute_locations;
}

TileGLUniformLocations GLShaderManager::tileUniformLocations() const
{
    return m_tile_uniform_location;
}

DebugGLAttributeLocations GLShaderManager::debugAttributeLocations() const
{
    return m_debug_attribute_locations;
}

DebugGLUniformLocations GLShaderManager::debugUniformLocations() const
{
    return m_debug_uniform_location;
}

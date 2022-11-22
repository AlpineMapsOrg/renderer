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

GLShaderManager::GLShaderManager()
{
    m_tile_program = std::make_unique<ShaderProgram>(
        ShaderProgram::Files({"gl_shaders/tile.vert"}),
        ShaderProgram::Files({"gl_shaders/tile.frag"}));
    m_debug_program = std::make_unique<ShaderProgram>(debugVertexShaderSource, debugFragmentShaderSource);
    m_screen_quad_program = std::make_unique<ShaderProgram>(
        ShaderProgram::Files({"gl_shaders/screen_pass.vert"}),
        ShaderProgram::Files({"gl_shaders/screen_copy.frag"}));
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

ShaderProgram* GLShaderManager::tileShader() const
{
    return m_tile_program.get();
}

ShaderProgram* GLShaderManager::debugShader() const
{
    return m_debug_program.get();
}

ShaderProgram* GLShaderManager::screen_quad_program() const
{
    return m_screen_quad_program.get();
}

void GLShaderManager::release()
{
    m_tile_program->release();
}

void GLShaderManager::reload_shaders()
{
    m_tile_program->reload();
    m_debug_program->reload();
    m_screen_quad_program->reload();
}

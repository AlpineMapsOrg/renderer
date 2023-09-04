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

#include "ShaderManager.h"

#include <QOpenGLContext>

#include "ShaderProgram.h"

using gl_engine::ShaderManager;
using gl_engine::ShaderProgram;


ShaderManager::ShaderManager()
{
    m_tile_program = std::make_unique<ShaderProgram>("tile.vert", "tile.frag");
    m_screen_quad_program = std::make_unique<ShaderProgram>("screen_pass.vert", "screen_copy.frag");
    m_atmosphere_bg_program = std::make_unique<ShaderProgram>("screen_pass.vert", "atmosphere_bg.frag");
    m_compose_program = std::make_unique<ShaderProgram>("screen_pass.vert", "compose.frag");
    m_ssao_program = std::make_shared<ShaderProgram>("screen_pass.vert", "ssao.frag");

    m_program_list.push_back(m_tile_program.get());
    m_program_list.push_back(m_screen_quad_program.get());
    m_program_list.push_back(m_atmosphere_bg_program.get());
    m_program_list.push_back(m_compose_program.get());
    m_program_list.push_back(m_ssao_program.get());
}

ShaderManager::~ShaderManager() = default;

ShaderProgram* ShaderManager::ssao_program() const
{
    return m_ssao_program.get();
}

ShaderProgram* ShaderManager::tile_shader() const
{
    return m_tile_program.get();
}

ShaderProgram* ShaderManager::compose_program() const
{
    return m_compose_program.get();
}

ShaderProgram* ShaderManager::screen_quad_program() const
{
    return m_screen_quad_program.get();
}

ShaderProgram* ShaderManager::atmosphere_bg_program() const
{
    return m_atmosphere_bg_program.get();
}

std::vector<ShaderProgram*> ShaderManager::all() const {
    return m_program_list;
}

void ShaderManager::release()
{
    m_tile_program->release();
}

void ShaderManager::reload_shaders()
{
    for (auto* program : m_program_list) {
        program->reload();
    }
}

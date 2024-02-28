 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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
    m_screen_copy = std::make_unique<ShaderProgram>("screen_pass.vert", "screen_copy.frag");
    m_atmosphere_bg_program = std::make_unique<ShaderProgram>("screen_pass.vert", "atmosphere_bg.frag");
    m_compose_program = std::make_unique<ShaderProgram>("screen_pass.vert", "compose.frag");
    m_ssao_program = std::make_shared<ShaderProgram>("screen_pass.vert", "ssao.frag");
    m_ssao_blur_program = std::make_shared<ShaderProgram>("screen_pass.vert", "ssao_blur.frag");
    m_shadowmap_program = std::make_unique<ShaderProgram>("shadowmap.vert", "shadowmap.frag");
    m_track_program = std::make_unique<ShaderProgram>("track.vert", "track.frag");
    m_labels_program = std::make_unique<ShaderProgram>("labels.vert", "labels.frag");

    m_program_list.push_back(m_tile_program.get());
    m_program_list.push_back(m_screen_copy.get());
    m_program_list.push_back(m_atmosphere_bg_program.get());
    m_program_list.push_back(m_compose_program.get());
    m_program_list.push_back(m_ssao_program.get());
    m_program_list.push_back(m_ssao_blur_program.get());
    m_program_list.push_back(m_shadowmap_program.get());
    m_program_list.push_back(m_track_program.get());
    m_program_list.push_back(m_labels_program.get());
}

ShaderManager::~ShaderManager() = default;

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

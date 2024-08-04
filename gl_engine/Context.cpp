/*****************************************************************************
 * Alpine Terrain Renderer
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

#include "Context.h"
#include "ShaderManager.h"
#include "TrackManager.h"
using namespace gl_engine;

Context::Context() { nucleus::EngineContext::set_singleton(this); }

Context::~Context() = default;

TrackManager* Context::track_manager()
{
    assert(is_alive());
    return m_track_manager.get();
}

ShaderManager* Context::shader_manager()
{
    assert(is_alive());
    return m_shader_manager.get();
}

void Context::internal_initialise()
{
    m_shader_manager = std::make_unique<ShaderManager>();
    m_track_manager = std::make_unique<TrackManager>(m_shader_manager.get());
}

void Context::internal_destroy()
{
    m_track_manager.reset();
    m_shader_manager.reset();
}

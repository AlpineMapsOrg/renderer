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
#include "MapLabelManager.h"
#include "ShaderRegistry.h"
#include "TrackManager.h"
using namespace gl_engine;

Context::Context(QObject* parent)
    : nucleus::EngineContext(parent)
{
}

Context::~Context() = default;

TrackManager* Context::track_manager()
{
    assert(is_alive());
    return m_track_manager.get();
}

ShaderRegistry* Context::shader_registry()
{
    assert(is_alive());
    return m_shader_registry.get();
}

void Context::internal_initialise()
{
    m_shader_registry = std::make_unique<ShaderRegistry>();
    m_track_manager = std::make_unique<TrackManager>(m_shader_registry.get());
    if (m_map_label_manager)
        m_map_label_manager->init(m_shader_registry.get());
}

void Context::internal_destroy()
{
    m_track_manager.reset();
    m_shader_registry.reset();
    if (m_map_label_manager)
        m_map_label_manager.reset();
}

gl_engine::MapLabelManager* Context::map_label_manager() const { return m_map_label_manager.get(); }

void Context::set_map_label_manager(std::unique_ptr<gl_engine::MapLabelManager> new_map_label_manager)
{
    assert(!is_alive()); // only set before init is called.
    m_map_label_manager = std::move(new_map_label_manager);
}

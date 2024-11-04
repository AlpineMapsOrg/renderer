/*****************************************************************************
 * AlpineMaps.org
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
#include "MapLabels.h"
#include "ShaderRegistry.h"
#include "TextureLayer.h"
#include "TileGeometry.h"
#include "TrackManager.h"
#include <QOpenGLContext>

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
    assert(!m_shader_registry);
    // init of shader registry and track manager should be moved out of here for more flexibility, similar to tile_geometry
    m_shader_registry = std::make_shared<ShaderRegistry>();
    m_track_manager = std::make_shared<TrackManager>(m_shader_registry.get());
    if (m_map_label_manager)
        m_map_label_manager->init(m_shader_registry.get());

    if (m_tile_geometry)
        m_tile_geometry->init();

    if (m_ortho_layer)
        m_ortho_layer->init(m_shader_registry.get());
}

void Context::internal_destroy()
{
    // this is necessary for a clean shutdown (and we want a clean shutdown for the ci integration test).
    m_ortho_layer.reset();
    m_tile_geometry.reset();
    m_track_manager.reset();
    m_shader_registry.reset();
    m_map_label_manager.reset();
}

TextureLayer* Context::ortho_layer() const { return m_ortho_layer.get(); }

void Context::set_ortho_layer(std::shared_ptr<TextureLayer> new_ortho_layer)
{
    assert(!is_alive()); // only set before init is called.
    m_ortho_layer = std::move(new_ortho_layer);
}

TileGeometry* Context::tile_geometry() const { return m_tile_geometry.get(); }

void Context::set_tile_geometry(std::shared_ptr<TileGeometry> new_tile_geometry)
{
    assert(!is_alive()); // only set before init is called.
    m_tile_geometry = std::move(new_tile_geometry);
}

gl_engine::MapLabels* Context::map_label_manager() const { return m_map_label_manager.get(); }

void Context::set_map_label_manager(std::shared_ptr<gl_engine::MapLabels> new_map_label_manager)
{
    assert(!is_alive()); // only set before init is called.
    m_map_label_manager = std::move(new_map_label_manager);
}

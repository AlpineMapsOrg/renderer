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
#include "TrackManager.h"
using namespace gl_engine;

Context::Context() { m_window = std::make_shared<gl_engine::Window>(); }

std::weak_ptr<nucleus::AbstractRenderWindow> Context::render_window() { return m_window; }

void Context::setup_tracks(nucleus::track::Manager* manager)
{
    connect(manager, &nucleus::track::Manager::tracks_changed, m_window->track_manager(), &TrackManager::change_tracks);
}

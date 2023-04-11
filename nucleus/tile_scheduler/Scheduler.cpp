/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "Scheduler.h"
#include "nucleus/tile_scheduler/utils.h"
#include "sherpa/quad_tree.h"

#include <QTimer>

using namespace nucleus::tile_scheduler;

Scheduler::Scheduler(QObject *parent)
    : QObject{parent}
{
    m_update_timer = std::make_unique<QTimer>();
    m_update_timer->setSingleShot(true);
    connect(m_update_timer.get(), &QTimer::timeout, this, &Scheduler::do_update);
}

Scheduler::~Scheduler() = default;

void Scheduler::update_camera(const camera::Definition& camera)
{
    m_current_camera = camera;
    schedule_update();
}

void Scheduler::receiver_quads(const std::vector<tile_types::TileQuad>& new_quads)
{
    m_ram_cache.insert(new_quads);
}

void Scheduler::do_update()
{
    auto currently_active_tiles = tiles_for_current_camera_position();
    std::erase_if(currently_active_tiles, [this](const tile::Id& id) {
        return m_ram_cache.contains(id);
    });
    emit(quads_requested(currently_active_tiles));
}

void Scheduler::schedule_update()
{
    if (m_enabled && !m_update_timer->isActive())
        m_update_timer->start(m_update_timeout);
}

std::vector<tile::Id> Scheduler::tiles_for_current_camera_position() const
{
    std::vector<tile::Id> all_inner_nodes;
    const auto all_leaves = quad_tree::onTheFlyTraverse(
        tile::Id { 0, { 0, 0 } },
        tile_scheduler::utils::refineFunctor(m_current_camera, m_aabb_decorator, m_permissible_screen_space_error, m_ortho_tile_size),
        [&all_inner_nodes](const tile::Id& v) { all_inner_nodes.push_back(v); return v.children(); });

    //    all_inner_nodes.reserve(all_inner_nodes.size() + all_leaves.size());
    //    std::copy(all_leaves.begin(), all_leaves.end(), std::back_inserter(all_inner_nodes));
    return all_inner_nodes;
}

void Scheduler::set_aabb_decorator(const utils::AabbDecoratorPtr& new_aabb_decorator)
{
    m_aabb_decorator = new_aabb_decorator;
}

void Scheduler::set_permissible_screen_space_error(float new_permissible_screen_space_error)
{
    m_permissible_screen_space_error = new_permissible_screen_space_error;
}

bool Scheduler::enabled() const
{
    return m_enabled;
}

void Scheduler::set_enabled(bool new_enabled)
{
    m_enabled = new_enabled;
    schedule_update();
}

unsigned int Scheduler::update_timeout() const
{
    return m_update_timeout;
}

void Scheduler::set_update_timeout(unsigned new_update_timeout)
{
    m_update_timeout = new_update_timeout;
    if (m_update_timer->isActive()) {
        m_update_timer->start(m_update_timeout);
    }
}

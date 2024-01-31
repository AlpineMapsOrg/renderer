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

#include "SlotLimiter.h"

using namespace nucleus::tile_scheduler;

SlotLimiter::SlotLimiter(QObject* parent)
    : QObject { parent }
{
}

void SlotLimiter::set_limit(unsigned new_limit)
{
    assert(new_limit > 0);
    m_limit = new_limit;
}

unsigned SlotLimiter::limit() const
{
    return m_limit;
}

unsigned SlotLimiter::slots_taken() const
{
    return unsigned(m_in_flight.size());
}

void SlotLimiter::request_quads(const std::vector<tile::Id>& ids)
{
    m_request_queue.clear();
    for (const tile::Id& id : ids) {
        if (m_in_flight.contains(id))
            continue;
        if (m_in_flight.size() >= m_limit) {
            m_request_queue.push_back(id);
        } else {
            m_in_flight.insert(id);
            emit quad_requested(id);
        }
    }
}

void SlotLimiter::deliver_quad(const tile_types::TileQuad& tile)
{
    m_in_flight.erase(tile.id);
    emit quad_delivered(tile);
    if (m_request_queue.empty())
        return;

    m_in_flight.insert(m_request_queue.front());
    emit quad_requested(m_request_queue.front());
    m_request_queue.erase(m_request_queue.cbegin());
}

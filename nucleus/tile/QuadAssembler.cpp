/*****************************************************************************
 * AlpineMaps.org
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

#include "QuadAssembler.h"

using namespace nucleus::tile;

QuadAssembler::QuadAssembler(QObject* parent)
    : QObject { parent }
{
}

size_t QuadAssembler::n_items_in_flight() const { return m_quads.size(); }

void QuadAssembler::load(const tile::Id& tile_id)
{
    m_quads[tile_id].id = tile_id;
    for (const auto& child_id : tile_id.children()) {
        emit tile_requested(child_id);
    }
}

void QuadAssembler::deliver_tile(const Data& tile)
{
    auto& quad = m_quads[tile.id.parent()];
    quad.tiles[quad.n_tiles++] = tile;
    if (quad.n_tiles == 4) {
        emit quad_loaded(quad);
        m_quads.erase(quad.id);
    }
}

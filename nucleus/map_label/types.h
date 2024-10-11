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

#pragma once

#include <nucleus/tile_scheduler/tile_types.h>

namespace nucleus::map_label {
struct PoiCollection {
    tile::Id id;
    vector_tile::PointOfInterestCollectionPtr data;
};
static_assert(tile_scheduler::tile_types::NamedTile<PoiCollection>);

struct PoiCollectionQuad {
    tile::Id id;
    std::array<PoiCollection, 4> tiles;
};
static_assert(tile_scheduler::tile_types::NamedTile<PoiCollectionQuad>);
} // namespace nucleus::map_label

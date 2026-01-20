/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "RectangularTileRegion.h"

namespace webgpu_engine::compute {

std::vector<radix::tile::Id> RectangularTileRegion::get_tiles() const
{
    assert(min.x <= max.x);
    assert(min.y <= max.y);
    std::vector<radix::tile::Id> tiles;
    tiles.reserve((max.x - min.x + 1) * (max.y - min.y + 1));
    for (unsigned x = min.x; x <= max.x; x++) {
        for (unsigned y = min.y; y <= max.y; y++) {
            tiles.emplace_back(radix::tile::Id { zoom_level, { x, y }, scheme });
        }
    }
    return tiles;
}

} // namespace webgpu_engine::compute

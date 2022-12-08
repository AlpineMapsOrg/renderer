/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include "srs.h"

constexpr unsigned int cSemiMajorAxis = 6378137;
constexpr double cEarthCircumference = 2 * 3.1415926535897932384626433 * cSemiMajorAxis;
constexpr double cOriginShift = cEarthCircumference / 2.0;

namespace srs {

tile::SrsBounds tile_bounds(const tile::Id& tile)
{
    const auto width_of_a_tile = cEarthCircumference / number_of_horizontal_tiles_for_zoom_level(tile.zoom_level);
    const auto height_of_a_tile = cEarthCircumference / number_of_vertical_tiles_for_zoom_level(tile.zoom_level);
    glm::dvec2 absolute_min = { -cOriginShift, -cOriginShift };
    const auto min = absolute_min + glm::dvec2 { tile.coords.x * width_of_a_tile, tile.coords.y * height_of_a_tile };
    const auto max = min + glm::dvec2 { width_of_a_tile, height_of_a_tile };
    return { min, max };
}

bool overlap(const tile::Id& a, const tile::Id& b)
{
    const auto& smaller_zoom_tile = (a.zoom_level < b.zoom_level) ? a : b;
    auto other = (a.zoom_level >= b.zoom_level) ? a : b;

    while (other.zoom_level != smaller_zoom_tile.zoom_level) {
        other.zoom_level--;
        other.coords /= 2;
    }

    return smaller_zoom_tile == other;
}

}

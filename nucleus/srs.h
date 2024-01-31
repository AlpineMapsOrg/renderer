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

#pragma once

#include <array>
#include <functional>

#include <glm/glm.hpp>

#include "radix/tile.h"

namespace nucleus::srs {
// the srs used for the alpine renderer is EPSG: 3857 (also called web mercator, spherical mercator).
// this coordinate system uses metres for coordiantes, 0m norhting is on the equator, 0m easting on the prime merridian
// Vienna is about 1 822 577 metres east and 6 141 664 metres north

// we use the TMS tile convention, where y-tile 0 is the southern most (like a conventional coordinate system, not like images).
// zoom level 0 has 1 tile, the number of tiles quadrupels on each level.
// This coordinate system is clamped at at 85 degrees north and south (https://epsg.io/3857),
// so the tiles do not go until the poles.


// for now it's the same, but if we ever switch to wgs84 / epsg 4326 / the one with 2 tiles in level zero
inline unsigned number_of_horizontal_tiles_for_zoom_level(unsigned z) { return 1 << z; }
inline unsigned number_of_vertical_tiles_for_zoom_level(unsigned z) { return 1 << z; }

tile::SrsBounds tile_bounds(const tile::Id& tile);
bool overlap(const tile::Id& a, const tile::Id& b);

glm::dvec2 lat_long_to_world(const glm::dvec2& lat_long);
glm::dvec3 lat_long_alt_to_world(const glm::dvec3& lat_long_alt);
glm::dvec2 world_to_lat_long(const glm::dvec2& world_pos);
glm::dvec3 world_to_lat_long_alt(const glm::dvec3& world_pos);

double haversine_distance(const glm::dvec2& lat_long_1, const glm::dvec2& lat_long_2);
}

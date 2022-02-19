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

#include "alpine_renderer/utils/geometry.h"

namespace srs {
// the srs used for the alpine renderer is EPSG: 3857 (also called web mercator, spherical mercator).
// this coordinate system uses metres for coordiantes, 0m norhting is on the equator, 0m easting on the prime merridian
// Vienna is about 1 822 577 metres east and 6 141 664 metres north

// we use the TMS tile convention, where y-tile 0 is the southern most (like a conventional coordinate system, not like images).
// zoom level 0 has 1 tile, the number of tiles quadrupels on each level.
// This coordinate system is clamped at at 85 degrees north and south (https://epsg.io/3857),
// so the tiles do not go until the poles.

namespace {
template <class T>
void hash_combine(std::size_t& seed, const T& v)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}
}

struct TileId {
  unsigned zoom_level = unsigned(-1);
  glm::uvec2 coords;
  friend bool operator==(const TileId&, const TileId&) = default;

  struct Hasher
  {
    size_t operator()(const TileId& tile) const
    {
      std::size_t seed = 0;
      hash_combine(seed, tile.zoom_level);
      hash_combine(seed, tile.coords.x);
      hash_combine(seed, tile.coords.y);
      return seed;
    }
  };
};
struct Bounds {
  glm::dvec2 min = {};
  glm::dvec2 max = {};
};

inline bool contains(const Bounds& bounds, const glm::dvec2& point) {
  return bounds.min.x < point.x && point.x < bounds.max.x &&
         bounds.min.y < point.y && point.y < bounds.max.y;
}

// for now it's the same, but if we ever switch to wgs84 / epsg 4326 / the one with 2 tiles in level zero
inline unsigned number_of_horizontal_tiles_for_zoom_level(unsigned z) { return 1 << z; }
inline unsigned number_of_vertical_tiles_for_zoom_level(unsigned z) { return 1 << z; }

Bounds tile_bounds(const TileId& tile);
std::array<TileId, 4> subtiles(const TileId& tile);

inline geometry::AABB<3, double> aabb(const srs::TileId& tile_id, double min_height, double max_height)
{
  const auto bounds = srs::tile_bounds(tile_id);
  return {.min = {bounds.min, min_height}, .max = {bounds.max, max_height}};
}

}

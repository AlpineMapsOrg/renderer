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

#include <cmath>

constexpr double pi = 3.1415926535897932384626433;
constexpr unsigned int cSemiMajorAxis = 6378137;
constexpr double cEarthCircumference = 2 * pi * cSemiMajorAxis;
constexpr double cOriginShift = cEarthCircumference / 2.0;

namespace nucleus::srs {

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

// edited from https://stackoverflow.com/questions/14329691/convert-latitude-longitude-point-to-a-pixels-x-y-on-mercator-projection
glm::dvec2 lat_long_to_world(const glm::dvec2& lat_long)
{
    const auto& latitude = lat_long.x;
    const auto& longitude = lat_long.y;
    // get x value
    const auto x = (longitude + 180) * (cOriginShift / 180) - cOriginShift;

    // convert from degrees to radians
    const auto latRad = latitude * pi / 180;

    // get y value
    const auto mercN = std::log(std::tan((pi / 4) + (latRad / 2)));
    const auto y = cOriginShift * mercN / pi;
    return { x, y };
}

glm::dvec2 world_to_lat_long(const glm::dvec2& world_pos)
{
    const auto mercN = world_pos.y * pi / cOriginShift;
    const auto latRad = 2.0 * (std::atan(exp(mercN)) - (pi / 4.0));
    const auto latitude = latRad * 180 / pi;
    const auto longitude = (world_pos.x + cOriginShift) / (cOriginShift / 180) - 180;

    return { latitude, longitude };
}

glm::dvec3 world_to_lat_long_alt(const glm::dvec3& world_pos) {
    auto lat_long = world_to_lat_long(world_pos);
    return { lat_long.x, lat_long.y, world_pos.z * cos(lat_long.x * pi / 180) };
}

glm::dvec3 lat_long_alt_to_world(const glm::dvec3& lat_long_alt)
{
    const auto world_xy = lat_long_to_world({ lat_long_alt.x, lat_long_alt.y });
    const auto lat_rad_ = lat_long_alt.x * pi / 180.0;
    return { world_xy.x, world_xy.y, lat_long_alt.z / std::abs(std::cos(lat_rad_)) };
}

}

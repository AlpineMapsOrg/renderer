/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "nucleus/srs.h"
#include "nucleus/tile/Cache.h"
#include "radix/height_encoding.h"

#include "nucleus/utils/image_loader.h"

namespace nucleus::tile::cache_queries {

inline float query_altitude(MemoryCache* cache, const glm::dvec2& lat_long)
{
    const auto world_space = srs::lat_long_to_world(lat_long);
    nucleus::tile::Data selected_tile;
    cache->visit([&](const nucleus::tile::DataQuad& tile) {
        for (const auto& t : tile.tiles) {
            if (srs::tile_bounds(t.id).contains(world_space) && t.network_info.status == NetworkInfo::Status::Good) {
                selected_tile = t;
                return true;
            }
        }
        return false;
    });
    assert(selected_tile.data);
    assert(selected_tile.data->size());

    const auto bounds = srs::tile_bounds(selected_tile.id);
    const auto uv = (world_space - bounds.min) / bounds.size();

    if (selected_tile.data && selected_tile.data->size()) {
        if (const auto height_tile_expected = nucleus::utils::image_loader::rgba8(*selected_tile.data)) {
            const auto& height_tile = height_tile_expected.value();
            const auto p = glm::uvec2(uint32_t(uv.x * height_tile.width()), uint32_t((1 - uv.y) * height_tile.height()));
            const auto px = height_tile.pixel(p);
            return radix::height_encoding::to_float(glm::u8vec3(px));
        }
    }
    assert(false);
    return 1000;
}

} // namespace nucleus::tile::cache_queries

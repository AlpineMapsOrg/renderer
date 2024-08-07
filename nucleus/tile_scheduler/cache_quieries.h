/*****************************************************************************
 * Alpine Terrain Renderer
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
#include "nucleus/tile_scheduler/Cache.h"
#include "radix/height_encoding.h"

#include "nucleus/utils/image_loader.h"

namespace nucleus::tile_scheduler::cache_queries {

// NOTE: UNTESTED since swap from QImage!
inline float query_altitude(MemoryCache* cache, const glm::dvec2& lat_long)
{
    const auto world_space = srs::lat_long_to_world(lat_long);
    nucleus::tile_scheduler::tile_types::TileQuad selected_quad;
    cache->visit([&](const nucleus::tile_scheduler::tile_types::TileQuad& tile) {
        if (srs::tile_bounds(tile.id).contains(world_space)) {
            selected_quad = tile;
            return true;
        }
        return false;
    });

    for (const auto& layered_tile : selected_quad.tiles) {
        if (srs::tile_bounds(layered_tile.id).contains(world_space)) {
            const auto bounds = srs::tile_bounds(layered_tile.id);
            const auto uv = (world_space - bounds.min) / bounds.size();

            if (layered_tile.height && layered_tile.height->size()) {
                const auto height_tile = nucleus::utils::image_loader::rgba8(*layered_tile.height);
                const auto p = glm::uvec2(
                    uint32_t(uv.x * height_tile.width()),
                    uint32_t((1 - uv.y) * height_tile.height())
                );
                const auto px = height_tile.pixel(p);
                return radix::height_encoding::to_float(glm::u8vec3(px));
            } else {
                return 1000;
            }
        }
    }

    assert(false);
    return 1000;
}

} // namespace nucleus::tile_scheduler::cache_queries

/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Lucas Dworschak
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

#include "Scheduler.h"
#include <nucleus/vector_tile/parse.h>

namespace nucleus::map_label {

Scheduler::Scheduler(const nucleus::tile::Scheduler::Settings& settings)
    : nucleus::tile::Scheduler(settings)
{
}

Scheduler::~Scheduler() = default;

void Scheduler::transform_and_emit(const std::vector<tile::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    std::vector<vector_tile::PoiTile> new_gpu_tiles;
    new_gpu_tiles.reserve(new_quads.size() * 4);
    for (const auto& data_quad : new_quads) {
        assert(data_quad.n_tiles == 4);
        for (const auto& data_tile : data_quad.tiles) {
            vector_tile::PoiTile gpu_tile;
            gpu_tile.id = data_tile.id;
            auto pois = nucleus::vector_tile::parse::points_of_interest(*data_tile.data, dataquerier().get());
            gpu_tile.data = std::make_shared<vector_tile::PointOfInterestCollection>(std::move(pois));
            new_gpu_tiles.emplace_back(gpu_tile);
        }
    };

    std::vector<tile::Id> deleted_tiles;
    deleted_tiles.reserve(deleted_quads.size() * 4);
    for (const auto& quad_id : deleted_quads) {
        for (const auto& tile_id : quad_id.children()) {
            deleted_tiles.push_back(tile_id);
        }
    }

    emit gpu_tiles_updated(new_gpu_tiles, deleted_tiles);
}

bool Scheduler::is_ready_to_ship(const nucleus::tile::DataQuad& quad) const
{
    assert(m_geometry_ram_cache);
    return m_geometry_ram_cache->contains(quad.id);
}

void Scheduler::set_geometry_ram_cache(nucleus::tile::MemoryCache* new_geometry_ram_cache) { m_geometry_ram_cache = new_geometry_ram_cache; }

} // namespace nucleus::map_label

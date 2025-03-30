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

#include "GeometryScheduler.h"

#include "utils.h"
#include <QDebug>
#include <nucleus/tile/conversion.h>
#include <nucleus/utils/error.h>
#include <nucleus/utils/image_loader.h>

namespace nucleus::tile {

GeometryScheduler::GeometryScheduler(const Settings& settings, unsigned int height_map_size)
    : nucleus::tile::Scheduler(settings)
    , m_default_raster(glm::uvec2(height_map_size), uint16_t(0))
{
}

GeometryScheduler::~GeometryScheduler() = default;

void GeometryScheduler::transform_and_emit(const std::vector<tile::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    // Tested larger geometry tiles (129x129) and switched back to smaller ones (65x65) for performance reasons (smaller ones are twice as fast).
    std::vector<GpuGeometryTile> new_gpu_tiles;
    new_gpu_tiles.reserve(new_quads.size() * 4);

    for (const auto& quad : new_quads) {
        for (const auto& tile : quad.tiles) {
            GpuGeometryTile gpu_tile;
            gpu_tile.id = tile.id;
            if (tile.data->size()) {
                // tile is available
                using namespace nucleus::utils;
                gpu_tile.surface = std::make_shared<const nucleus::Raster<uint16_t>>(
                    image_loader::rgba8(*tile.data).and_then(error::wrap_to_expected(conversion::to_u16raster)).value_or(m_default_raster));
            } else {
                // tile is not available (use default tile)
                gpu_tile.surface = std::make_shared<const nucleus::Raster<uint16_t>>(m_default_raster);
            }
            new_gpu_tiles.push_back(gpu_tile);
        }
    }

    std::vector<tile::Id> deleted_tiles;
    deleted_tiles.reserve(deleted_quads.size() * 4);
    for (const auto& id : deleted_quads) {
        for (const auto& chid : id.children()) {
            deleted_tiles.push_back(chid);
        }
    }

    emit gpu_tiles_updated(deleted_tiles, new_gpu_tiles);
}

} // namespace nucleus::tile

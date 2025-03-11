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

GeometryScheduler::GeometryScheduler()
    : nucleus::tile::Scheduler(256)
    , m_default_raster(glm::uvec2(65), uint16_t(0))
{
}

GeometryScheduler::~GeometryScheduler() = default;

void GeometryScheduler::transform_and_emit(const std::vector<tile::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    std::vector<GpuGeometryTile> new_gpu_tiles;
    new_gpu_tiles.reserve(new_gpu_tiles.size() * 4);

    for (const auto& quad : new_quads) {
        GpuGeometryTile gpu_tile;
        gpu_tile.id = quad.id;
        gpu_tile.surface = std::make_shared<Raster<uint16_t>>(to_raster(quad, m_default_raster));
        new_gpu_tiles.push_back(gpu_tile);
    }

    // we are merging the tiles. so deleted quads become deleted tiles.
    emit gpu_tiles_updated(deleted_quads, new_gpu_tiles);
}

Raster<uint16_t> GeometryScheduler::to_raster(const tile::DataQuad& quad, const Raster<uint16_t>& default_raster)
{
    using namespace nucleus::utils;
    assert(quad.n_tiles == 4);

    std::array<Raster<uint16_t>, 4> quad_rasters;
    std::array<tile::Id, 4> quad_ids;
    for (const auto& tile : quad.tiles) {
        const auto quad_index = unsigned(quad_position(tile.id));
        quad_ids[quad_index] = tile.id;
        if (tile.data->size()) {
            // tile is available
            auto height_raster = image_loader::rgba8(*tile.data).and_then(error::wrap_to_expected(conversion::to_u16raster)).value_or(default_raster);
            quad_rasters[quad_index] = std::move(height_raster);
        } else {
            // tile is not available (use default tile)
            quad_rasters[quad_index] = default_raster;
        }
    }

    const auto out_size = default_raster.width() * 2 - 1;
    Raster<uint16_t> out(out_size);
    const auto& tl = quad_rasters[unsigned(tile::QuadPosition::TopLeft)];
    const auto& tr = quad_rasters[unsigned(tile::QuadPosition::TopRight)];
    const auto& bl = quad_rasters[unsigned(tile::QuadPosition::BottomLeft)];
    const auto& br = quad_rasters[unsigned(tile::QuadPosition::BottomRight)];
    const auto middle = out_size / 2;

    for (unsigned row = 0; row < middle; ++row) {
        for (unsigned col = 0; col < middle; ++col) {
            out.pixel({ col, row }) = tl.pixel({ col, row });
        }
        for (unsigned col = middle; col < out_size; ++col) {
            out.pixel({ col, row }) = tr.pixel({ col - middle, row });
        }
    }
    for (unsigned row = middle; row < out_size; ++row) {
        for (unsigned col = 0; col < middle; ++col) {
            out.pixel({ col, row }) = bl.pixel({ col, row - middle });
        }
        for (unsigned col = middle; col < out_size; ++col) {
            out.pixel({ col, row }) = br.pixel({ col - middle, row - middle });
        }
    }

    return out;
}

} // namespace nucleus::tile

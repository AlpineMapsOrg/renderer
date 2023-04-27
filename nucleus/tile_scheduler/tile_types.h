/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include <concepts>

#include "nucleus/tile_scheduler/utils.h"
#include "sherpa/tile.h"

#include <QByteArray>

class QImage;
namespace nucleus {
template <typename T>
class Raster;
}

namespace nucleus::tile_scheduler::tile_types {

template <typename T>
concept NamedTile = requires(T t) {
    {
        t.id
    } -> utils::convertible_to<tile::Id>;
};

struct LayeredTile {
    tile::Id id;
    std::shared_ptr<const QByteArray> ortho;
    std::shared_ptr<const QByteArray> height;
};
static_assert(NamedTile<LayeredTile>);

struct TileQuad {
    tile::Id id;
    unsigned n_tiles = 0;
    std::array<LayeredTile, 4> tiles;
};
static_assert(NamedTile<TileQuad>);

struct GpuCacheInfo {
    tile::Id id;
};
static_assert(NamedTile<GpuCacheInfo>);

struct GpuLayeredTile {
    tile::Id id;
    tile::SrsAndHeightBounds bounds = {};
    std::shared_ptr<const QImage> ortho;
    std::shared_ptr<const nucleus::Raster<uint16_t>> height;
};
static_assert(NamedTile<LayeredTile>);

struct GpuTileQuad {
    tile::Id id;
    std::array<GpuLayeredTile, 4> tiles;
};
static_assert(NamedTile<GpuTileQuad>);

} // namespace nucleus::tile_scheduler::tile_types

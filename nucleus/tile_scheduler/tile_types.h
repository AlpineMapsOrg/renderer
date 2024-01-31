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

#include "nucleus/tile_scheduler/utils.h"
#include "radix/tile.h"

#include <QByteArray>

class QImage;
namespace nucleus {
template <typename T>
class Raster;
}

namespace nucleus::tile_scheduler::tile_types {

struct NetworkInfo {
    enum class Status : uint64_t { // NetworkInfo will be padded by 4 bytes even when usign 32bit int. clear the warning.
        Good = 0,
        NotFound = 1,
        NetworkError = 2,

    };
    Status status;
    uint64_t timestamp;
    template <typename... Ts>
    static NetworkInfo join(const Ts&... infos)
    {
        const auto info_array = std::array<NetworkInfo, sizeof...(Ts)>{infos...};
        Status status = Status::Good;
        uint64_t timestamp = std::numeric_limits<uint64_t>::max();
        for (const auto& i : info_array) {
            status = std::max(status, i.status);
            timestamp = std::min(timestamp, i.timestamp);
        }
        return {status, timestamp};
    }

};

template <typename T>
concept NamedTile = requires(T t) {
    {
        t.id
    } -> utils::convertible_to<tile::Id>;
};

template <typename T>
concept SerialisableTile = requires(T t) {
    requires std::is_same<std::remove_reference_t<decltype(T::version_information)>, const std::array<char, 25>>::value;
};

struct TileLayer {
    tile::Id id;
    NetworkInfo network_info;
    std::shared_ptr<QByteArray> data;
};
static_assert(NamedTile<TileLayer>);

struct LayeredTile {
    tile::Id id;
    NetworkInfo network_info;
    std::shared_ptr<QByteArray> ortho;
    std::shared_ptr<QByteArray> height;
};
static_assert(NamedTile<LayeredTile>);

struct TileQuad {
    tile::Id id;
    unsigned n_tiles = 0;
    std::array<LayeredTile, 4> tiles = {};
    NetworkInfo network_info() const {
        return NetworkInfo::join(tiles[0].network_info, tiles[1].network_info, tiles[2].network_info, tiles[3].network_info);
    }
    static constexpr std::array<char, 25> version_information = {"TileQuad, version 0.3"};
};
static_assert(NamedTile<TileQuad>);
static_assert(SerialisableTile<TileQuad>);

struct GpuCacheInfo {
    tile::Id id;
};
static_assert(NamedTile<GpuCacheInfo>);

struct GpuLayeredTile {
    tile::Id id;
    tile::SrsAndHeightBounds bounds = {};
    std::shared_ptr<const QImage> ortho;
    std::shared_ptr<const nucleus::Raster<uint16_t>> height;
    std::shared_ptr<const QImage> height_image;
};
static_assert(NamedTile<GpuLayeredTile>);

struct GpuTileQuad {
    tile::Id id;
    std::array<GpuLayeredTile, 4> tiles;
};
static_assert(NamedTile<GpuTileQuad>);

} // namespace nucleus::tile_scheduler::tile_types

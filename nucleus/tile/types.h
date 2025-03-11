/*****************************************************************************
 * AlpineMaps.org
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

#include <QByteArray>

#include <nucleus/utils/ColourTexture.h>
#include <nucleus/utils/lang.h>
#include <radix/tile.h>

namespace nucleus {
template <typename T>
class Raster;
}
namespace nucleus::tile {
using namespace radix::tile;

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
    { t.id } -> nucleus::utils::convertible_to<tile::Id>;
};

template <typename T>
concept SerialisableTile = requires(T t) {
    requires std::is_same<std::remove_reference_t<decltype(T::version_information)>, const std::array<char, 25>>::value;
};

struct Data {
    tile::Id id;
    NetworkInfo network_info;
    std::shared_ptr<QByteArray> data;
};
static_assert(NamedTile<Data>);

struct DataQuad {
    tile::Id id;
    unsigned n_tiles = 0;
    std::array<Data, 4> tiles = {};
    NetworkInfo network_info() const { return NetworkInfo::join(tiles[0].network_info, tiles[1].network_info, tiles[2].network_info, tiles[3].network_info); }
    static constexpr std::array<char, 25> version_information = { "DataQuad, version 0.1" };
};
static_assert(NamedTile<DataQuad>);
static_assert(SerialisableTile<DataQuad>);

struct GpuCacheInfo {
    tile::Id id;
};
static_assert(NamedTile<GpuCacheInfo>);

struct GpuTextureTile {
    tile::Id id;
    std::shared_ptr<const nucleus::utils::MipmappedColourTexture> texture;
};
static_assert(NamedTile<GpuTextureTile>);

struct TileBounds {
    tile::Id id;
    tile::SrsAndHeightBounds bounds = {};
};

struct GpuGeometryTile {
    tile::Id id;
    tile::SrsAndHeightBounds bounds = {};
    std::shared_ptr<const nucleus::Raster<uint16_t>> surface;
};
static_assert(NamedTile<GpuGeometryTile>);

} // namespace nucleus::tile::tile_types

/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "radix/tile.h"
#include <cstdint>
#include <limits>

namespace webgpu_engine::compute {

struct GpuTileId {
    uint32_t x;
    uint32_t y;
    uint32_t zoomlevel;
    uint32_t alignment = std::numeric_limits<uint32_t>::max();

    GpuTileId() = default;
    GpuTileId(uint32_t x, uint32_t y, uint32_t zoomlevel);
    GpuTileId(const radix::tile::Id& tile_id);

    bool operator==(const GpuTileId& other) const { return x == other.x && y == other.y && zoomlevel == other.zoomlevel; }
};

} // namespace webgpu_engine::compute

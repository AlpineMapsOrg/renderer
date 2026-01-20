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

#include "GpuTileId.h"

namespace webgpu_engine::compute {

GpuTileId::GpuTileId(uint32_t x, uint32_t y, uint32_t zoomlevel)
    : x { x }
    , y { y }
    , zoomlevel { zoomlevel }
{
}

GpuTileId::GpuTileId(const radix::tile::Id& tile_id)
    : x { tile_id.coords.x }
    , y { tile_id.coords.y }
    , zoomlevel { tile_id.zoom_level }
{
}

} // namespace webgpu_engine::compute

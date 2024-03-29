/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Adam Celerek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include <vector>

#include "radix/tile.h"

// we want to be flexible and have the ability to draw several tiles at once.
// GpuTileSets can have an arbitrary number of slots, each slot is an index in the corresponding
// vao buffers and textures.

namespace webgpu_engine {
struct TileSet {
    tile::Id tile_id = {};
    tile::SrsBounds bounds = {};
    unsigned texture_layer = unsigned(-1);
    // texture
};
} // namespace webgpu_engine

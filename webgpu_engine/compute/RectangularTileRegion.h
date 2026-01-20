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
#include <vector>

namespace webgpu_engine::compute {

struct RectangularTileRegion {
    glm::uvec2 min;
    glm::uvec2 max;
    unsigned int zoom_level;
    radix::tile::Scheme scheme;

    std::vector<radix::tile::Id> get_tiles() const;
};

} // namespace webgpu_engine::compute

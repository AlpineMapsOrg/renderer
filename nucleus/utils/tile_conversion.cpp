/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "tile_conversion.h"

namespace nucleus::utils::tile_conversion {

Raster<uint16_t> to_u16raster(const Raster<glm::u8vec4>& raster)
{
    Raster<uint16_t> retval(raster.size());
    std::transform(raster.begin(), raster.end(), retval.begin(), [](const glm::u8vec4& rgba) {
        return uint16_t(rgba.x) << 8 | uint16_t(rgba.y);
    });
    return retval;
}

}

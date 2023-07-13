/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include "Definition.h"
#include "nucleus/srs.h"

namespace nucleus::camera::stored_positions {
// coordinate transformer: https://epsg.io/transform#s_srs=4326&t_srs=3857&x=16.3726561&y=48.2086939
inline nucleus::camera::Definition oestl_hochgrubach_spitze()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.5587933, 12.3450985, 2277.0 });
    return { { coords.x, coords.y - 500, coords.z + 500 }, coords };
}
inline nucleus::camera::Definition stephansdom()
{
    const auto coords = srs::lat_long_alt_to_world({ 48.20851144787232, 16.373082444395656, 171.28 });
    return {{coords.x, coords.y - 500, coords.z + 500}, coords};
}
inline nucleus::camera::Definition stephansdom_closeup()
{
    const auto coords = srs::lat_long_alt_to_world({48.20851144787232, 16.373082444395656, 171.28});
    return {{coords.x, coords.y - 100, coords.z + 100}, coords};
}

inline nucleus::camera::Definition grossglockner()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.07386676653372, 12.694470292406267, 3798 });
    return { { coords.x - 300, coords.y - 400, coords.z + 100 }, { coords.x, coords.y, coords.z - 100 } };
}

inline nucleus::camera::Definition schneeberg()
{
    const auto coords = srs::lat_long_alt_to_world({47.767163598, 15.804663448, 2076});
    return {{coords.x + 3000, coords.y - 100, coords.z + 100}, {coords.x, coords.y, coords.z - 100}};
}

inline nucleus::camera::Definition karwendel()
{
    const auto coords = srs::lat_long_alt_to_world({47.416665, 11.4666648, 2000});
    return {{coords.x + 5000, coords.y + 2000, coords.z + 1000},
            {coords.x, coords.y, coords.z - 100}};
}
inline nucleus::camera::Definition wien()
{
    const auto coords = srs::lat_long_alt_to_world({48.20851144787232, 16.373082444395656, 171.28});
    return {{coords.x + 10'000, coords.y + 2'000, coords.z + 1'000}, coords};
}
} // namespace nucleus::camera::stored_positions

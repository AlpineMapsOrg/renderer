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
    return { { coords.x, coords.y - 500, coords.z + 500 }, coords };
}
inline nucleus::camera::Definition grossglockner()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.07386676653372, 12.694470292406267, 3798 });
    return { { coords.x - 300, coords.y - 400, coords.z + 100 }, { coords.x, coords.y, coords.z - 100 } };
}
inline nucleus::camera::Definition dachstein()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.47519, 13.60569, 2995 });
    return { { coords.x, coords.y - 1000, coords.z + 300 }, { coords.x, coords.y, coords.z - 100 } };
}
inline nucleus::camera::Definition roadA()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.73963, 13.43980, 542 });
    return { { coords.x, coords.y - 400, coords.z + 100 }, { coords.x, coords.y, coords.z } };
}
inline nucleus::camera::Definition roadB()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.14249, 12.81251, 1660 });
    return { { coords.x, coords.y + 300, coords.z + 70 }, { coords.x, coords.y, coords.z } };
}
inline nucleus::camera::Definition roadC()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.06347, 12.81843, 1870 });
    return { { coords.x - 100, coords.y - 200, coords.z + 50 }, { coords.x, coords.y, coords.z } };
}
inline nucleus::camera::Definition grubenkarspitzeA()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.38078, 11.52211, 2663 });
    return { { coords.x + 300, coords.y + 400, coords.z + 200 }, { coords.x, coords.y, coords.z - 20} };
}
inline nucleus::camera::Definition grubenkarspitzeB()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.38078, 11.52211, 2663 });
    return { { coords.x + 300, coords.y - 300, coords.z + 200 }, { coords.x, coords.y, coords.z - 20 } };
}
inline nucleus::camera::Definition grubenkarspitzeC()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.38078, 11.52211, 2663 });
    return { { coords.x - 400, coords.y + 100, coords.z + 200 }, { coords.x, coords.y, coords.z - 20 } };
}
inline nucleus::camera::Definition ravineA()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.85239, 15.28817, 630 });
    return { { coords.x + 80, coords.y + 80, coords.z + 100 }, { coords.x, coords.y, coords.z } };
}
inline nucleus::camera::Definition ravineB()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.85621, 15.28423, 610 });
    return { { coords.x + 100, coords.y, coords.z + 100 }, { coords.x, coords.y, coords.z } };
}
inline nucleus::camera::Definition ravineC()
{
    const auto coords = srs::lat_long_alt_to_world({ 48.37488, 13.94198, 300 });
    return { { coords.x, coords.y - 100, coords.z + 100 }, { coords.x, coords.y, coords.z } };
}
inline nucleus::camera::Definition pathA()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.50127, 10.61249, 2176 });
    return { { coords.x, coords.y - 200, coords.z + 4000 }, { coords.x, coords.y, coords.z } };
}
inline nucleus::camera::Definition pathB()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.45723, 11.77804, 2261 });
    return { { coords.x, coords.y - 200, coords.z + 4000 }, { coords.x, coords.y, coords.z } };
}
inline nucleus::camera::Definition pathC()
{
    const auto coords = srs::lat_long_alt_to_world({ 47.04033, 10.51186, 3004 });
    return { { coords.x, coords.y - 200, coords.z + 4000 }, { coords.x, coords.y, coords.z } };
}
}

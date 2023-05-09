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
inline nucleus::camera::Definition sankt_wolfgang()
{
    return { { 1497054.5684504572, 6063356.00872615 - 500, 548 + 100 }, { 1497054.5684504572, 6063356.00872615, 548 } };
}
inline nucleus::camera::Definition grubenkarspitze()
{
    return { { 1282635.4180640853 + 300, 6004450.05623309 + 400, 2663 + 200 }, { 1282635.4180640853, 6004450.05623309, 2663 } };
}
inline nucleus::camera::Definition oetschergraeben()
{
    return { { 1701871.2995610011 + 100, 6082332.828420961 + 100, 630 + 200 }, { 1701871.2995610011, 6082332.828420961, 630 } };
}
inline nucleus::camera::Definition gimpel()
{
    return { { 1181376.9828487078 - 900, 6024281.384489921 + 400, 2176 + 200 }, { 1181376.9828487078, 6024281.384489921, 2176 - 50 } };
}
}

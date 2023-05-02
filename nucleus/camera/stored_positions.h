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

namespace nucleus::camera::stored_positions {
// coordinate transformer: https://epsg.io/transform#s_srs=4326&t_srs=3857&x=16.3726561&y=48.2086939
inline nucleus::camera::Definition westl_hochgrubach_spitze()
{
    return { { 1374755.7, 6033683.4 - 500, 2277.0 + 500 }, { 1374755.7, 6033683.4, 2277.0 } };
}
inline nucleus::camera::Definition stephansdom()
{
    return { { 1822577.0, 6141664.0 - 500, 171.28 + 500 }, { 1822577.0, 6141664.0, 171.28 } };
}
inline nucleus::camera::Definition grossglockner()
{
    return { { 1413228.4559138024 - 300, 5954302.742129561 - 400, 3798 + 100 }, { 1413228.4559138024, 5954302.742129561, 3798 - 100 } };
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

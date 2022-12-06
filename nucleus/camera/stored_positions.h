/*****************************************************************************
 * Alpine Renderer
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

namespace camera::stored_positions {
// coordinate transformer: https://epsg.io/transform#s_srs=4326&t_srs=3857&x=16.3726561&y=48.2086939
inline camera::Definition westl_hochgrubach_spitze()
{
    return { { 1374755.7, 6033683.4 - 500, 2277.0 + 500 }, { 1374755.7, 6033683.4, 2277.0 } };
}
inline camera::Definition stephansdom()
{
    return { { 1822577.0, 6141664.0 - 500, 171.28 + 500 }, { 1822577.0, 6141664.0, 171.28 } };
}
}

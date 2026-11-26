/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Adam Celarek
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

namespace nucleus::tile::constants {

constexpr auto default_network_timeout = 8000;
using TimestampType = unsigned;

// the dem tile server: terrain geometry for the renderer and the dem fetches
// of the route planner. the previous "alpine_png" dataset no longer exists on
// the server; at_dtm_alpinemaps is the austrian DTM, so terrain data is only
// available within austria's coverage
inline constexpr auto dem_tile_server_url = "https://alpinemaps.cg.tuwien.ac.at/tiles/at_dtm_alpinemaps/";

}

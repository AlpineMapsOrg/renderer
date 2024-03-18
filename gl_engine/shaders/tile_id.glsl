/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
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

mediump uint hash_tile_id(mediump uint zoom_level, mediump uint coord_x, mediump uint coord_y) {
    mediump uint z = zoom_level * 46965u + 10859u;
    mediump uint x = coord_x * 60197u + 12253u;
    mediump uint y = coord_y * 62117u + 59119u;
    return (x + y + z) & 65535u;
}

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

highp uvec2 pack_tile_id(highp uint zoom_level, highp uint coord_x, highp uint coord_y) {
    highp uint a = zoom_level << (32 - 5);
    a = a | (coord_x >> 3);
    highp uint b = coord_x << (32 - 3);
    b = b | coord_y;
    return uvec2(a, b);
}

highp uvec3 unpack_tile_id(highp uvec2 packed_id) {
    highp uvec3 id;
    id.z = packed_id.x >> (32u - 5u);
    id.x = (packed_id.x & ((1u << (32u - 5u)) - 1u)) << 3u;
    id.x = id.x | (packed_id.y >> (32u - 3u));
    id.y = packed_id.y & ((1u << (32u - 3u)) - 1u);
    return id;
}

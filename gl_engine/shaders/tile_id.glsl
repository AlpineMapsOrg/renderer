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

mediump uint hash_tile_id(mediump uvec3 id) {
    mediump uint z = id.z * 46965u + 10859u;
    mediump uint x = id.x * 60197u + 12253u;
    mediump uint y = id.y * 62117u + 59119u;
    return (x + y + z) & 65535u;
}

highp uvec2 pack_tile_id(highp uvec3 id) {
    highp uint a = id.z << (32 - 5);
    a = a | (id.x >> 3);
    highp uint b = id.x << (32 - 3);
    b = b | id.y;
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

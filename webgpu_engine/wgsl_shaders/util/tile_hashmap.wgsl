/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Patrick Komon
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

#include "util/filtering.wgsl"

fn hash_tile_id_vec(id: vec3<u32>) -> u32 {
    let z = id.z * 46965u + 10859u;
    let x = id.x * 60197u + 12253u;
    let y = id.y * 62117u + 59119u;
    return (x + y + z) & 65535u;
}

fn hash_tile_id(id: TileId) -> u32 {
    return hash_tile_id_vec(vec3<u32>(id.x, id.y, id.zoomlevel));
}


fn get_texture_array_index(tile_id: TileId, texture_array_index: ptr<function, u32>, map_key_buffer: ptr<storage, array<TileId>>, map_value_buffer: ptr<storage, array<u32>>)  -> bool {
    // find correct hash for tile id
    var hash = hash_tile_id(tile_id);
    while(!tile_ids_equal(map_key_buffer[hash], tile_id) && !tile_id_empty(map_key_buffer[hash])) {
        hash++;
    }

    let was_found = !tile_id_empty(map_key_buffer[hash]);
    if (was_found) {
        // retrieve array layer by hash
        *texture_array_index = map_value_buffer[hash];
    } else {
        *texture_array_index = 0;
    }
    return was_found;
}

fn get_neighboring_tile_id_and_pos(texture_size: u32, tile_id: TileId, pos: vec2<i32>, out_tile_id: ptr<function, TileId>, out_pos: ptr<function, vec2<u32>>) {
    var new_pos = pos;
    var new_tile_id = tile_id;
    
    // tiles overlap! in each direction, the last value of one tile equals the first of the next
    // therefore offset by 1 in respective direction
    if (new_pos.x == -1) {
        new_pos.x = i32(texture_size - 2);
        new_tile_id.x -= 1;
    } else if (new_pos.x == i32(texture_size)) {
        new_pos.x = 1;
        new_tile_id.x += 1;
    }
    if (new_pos.y == -1) {
        new_pos.y = i32(texture_size - 2);
        new_tile_id.y += 1;
    } else if (new_pos.y == i32(texture_size)) {
        new_pos.y = 1;
        new_tile_id.y -= 1;
    }

    *out_tile_id = new_tile_id;
    *out_pos = vec2u(new_pos);
}

// currently unused
fn sample_height_by_uv(
    tile_id: TileId,
    uv: vec2f,
    map_key_buffer: ptr<storage, array<TileId>>,
    map_value_buffer: ptr<storage, array<u32>>,
    height_tiles_texture: texture_2d_array<u32>,
    height_tiles_sampler: sampler
) -> u32 {
    var texture_array_index: u32;
    let found = get_texture_array_index(tile_id, &texture_array_index, map_key_buffer, map_value_buffer);
    return select(0, bilinear_sample_u32(height_tiles_texture, height_tiles_sampler, uv, texture_array_index), found);
}

fn load_height_by_position(
    tile_id: TileId,
    pos: vec2u,
    map_key_buffer: ptr<storage, array<TileId>>,
    map_value_buffer: ptr<storage, array<u32>>,
    height_textures: texture_2d_array<u32>
) -> u32 {
    var texture_array_index: u32;
    let found = get_texture_array_index(tile_id, &texture_array_index, map_key_buffer, map_value_buffer);
    return select(0, textureLoad(height_textures, pos, texture_array_index, 0).r, found);
}

fn load_height_with_neighbors(
    tile_id: TileId,
    pos: vec2i,
    map_key_buffer: ptr<storage, array<TileId>>,
    map_value_buffer: ptr<storage, array<u32>>,
    height_tiles_texture: texture_2d_array<u32>,
) -> u32 {
    let height_texture_size = textureDimensions(height_tiles_texture);
    var target_tile_id: TileId;
    var target_pos: vec2u;
    get_neighboring_tile_id_and_pos(height_texture_size.x, tile_id, pos, &target_tile_id, &target_pos);

    return load_height_by_position(target_tile_id, target_pos, map_key_buffer, map_value_buffer, height_tiles_texture);
}

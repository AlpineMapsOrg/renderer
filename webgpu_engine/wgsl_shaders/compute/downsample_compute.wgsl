/*****************************************************************************
 * weBIGeo
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

#include "util/tile_util.wgsl"
#include "util/tile_hashmap.wgsl"

// input
@group(0) @binding(0) var<storage> input_tile_ids: array<TileId>;
@group(0) @binding(1) var<storage> map_key_buffer: array<TileId>; // hash map key buffer
@group(0) @binding(2) var<storage> map_value_buffer: array<u32>; // hash map value buffer, contains texture array indices
@group(0) @binding(3) var input_textures: texture_2d_array<f32>; // textures per input tile

// output
@group(0) @binding(4) var output_textures: texture_storage_2d_array<rgba8unorm, write>; // textures per downsampled tile

// NOTE: read_write binding for storage texture of type rgba8unorm currently IS NOT supported by webGPU.
// There is ongoing discussion, but for now, we write to a different texture and copy afterwards as a workaround.

fn sample_higher_zoomlevel_tile(tile_id: TileId, coords: vec2<u32>, size: u32) -> vec4<f32> {
    // get tile id of tile to sample from for current position
    var higher_zoomlevel_tile_id: TileId;
    higher_zoomlevel_tile_id.x = 2u * tile_id.x + select(0u, 1u, coords.x >= size);
    higher_zoomlevel_tile_id.y = 2u * tile_id.y + select(0u, 1u, coords.y < size);
    higher_zoomlevel_tile_id.zoomlevel = tile_id.zoomlevel + 1;

    var texture_array_index: u32;
    let found = get_texture_array_index(higher_zoomlevel_tile_id, &texture_array_index, &map_key_buffer, &map_value_buffer);
    if (!found) {
        return vec4f(0);
    }
    return textureLoad(input_textures, coords % vec2u(size), texture_array_index, 0);
}



// Function to perform the compare-and-swap operation
fn compare_and_swap(a: ptr<function, f32>, b: ptr<function, f32>) {
    let temp = *a;
    if (*a > *b) {
        *a = *b;
        *b = temp;
    }
}

// Function to sort a vec4 using a sorting network
fn sort_vec4(input: vec4<f32>) -> vec4<f32> {
    var a = input.x;
    var b = input.y;
    var c = input.z;
    var d = input.w;
    var data = array<f32, 4>(input.x, input.y, input.z, input.w);

    // Sorting network for 4 elements
    compare_and_swap(&a, &b);
    compare_and_swap(&c, &d);
    compare_and_swap(&a, &c);
    compare_and_swap(&b, &d);
    compare_and_swap(&b, &c);

    return vec4<f32>(a, b, c, d);
}

fn median(input: vec4<f32>) -> f32 {
    let sorted = sort_vec4(input);
    return (sorted.y + sorted.z) / 2;
}

fn aggregate_linear(sample_00: vec4f, sample_01: vec4f, sample_10: vec4f, sample_11: vec4f) -> vec4f {
    /*let avg = (2 * sample_00 - 1
        + 2 * sample_01 - 1
        + 2 * sample_10 - 1
        + 2 * sample_11 - 1) * 0.25;
    return vec4f(normalize(avg.xyz), avg.w) * 0.5 + 0.5;*/
    
    // this leads to normals that seem to be getting progressively shorter on lower levels, maybe an accuracy thing?
    // when using higher resolutions (i.e. 256 instead of 65), the difference is not visible
    // EDIT: seems to have been fixed by some other change, but leaving the comment here for now
    return (sample_00 + sample_01 + sample_10 + sample_11) * 0.25f;
}

fn aggregate_max(sample_00: vec4f, sample_01: vec4f, sample_10: vec4f, sample_11: vec4f) -> vec4f {
    return max(sample_00, max(sample_01, max(sample_10, sample_11)));
}

fn aggregate_min(sample_00: vec4f, sample_01: vec4f, sample_10: vec4f, sample_11: vec4f) -> vec4f {
    return min(sample_00, min(sample_01, min(sample_10, sample_11)));
}

fn aggregate_median(sample_00: vec4f, sample_01: vec4f, sample_10: vec4f, sample_11: vec4f) -> vec4f {
    return vec4f(median(vec4f(sample_00.x, sample_01.x, sample_10.x, sample_11.x)),
        median(vec4f(sample_00.y, sample_01.y, sample_10.y, sample_11.y)),
        median(vec4f(sample_00.z, sample_01.z, sample_10.z, sample_11.z)),
        median(vec4f(sample_00.w, sample_01.w, sample_10.w, sample_11.w)));
}

@compute @workgroup_size(1, 16, 16)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let tile_id = input_tile_ids[id.x];
    let size = textureDimensions(output_textures).x; // TODO allow non-square
    
    // exit if thread id is outside image dimensions (i.e. thread is not supposed to be doing any work)
    if (id.y >= size || id.z >= size) {
        return;
    }
    
    let final_coord = vec2<u32>(id.y, id.z);
    
    let coords_sample_00 = (2 * final_coord + vec2u(0, 0));
    let coords_sample_01 = (2 * final_coord + vec2u(0, 1));
    let coords_sample_10 = (2 * final_coord + vec2u(1, 0));
    let coords_sample_11 = (2 * final_coord + vec2u(1, 1));
    
    let sample_00 = sample_higher_zoomlevel_tile(tile_id, coords_sample_00, size);
    let sample_01 = sample_higher_zoomlevel_tile(tile_id, coords_sample_01, size);
    let sample_10 = sample_higher_zoomlevel_tile(tile_id, coords_sample_10, size);
    let sample_11 = sample_higher_zoomlevel_tile(tile_id, coords_sample_11, size);

    let final_value = aggregate_linear(sample_00, sample_01, sample_10, sample_11);

    textureStore(output_textures, final_coord, id.x, final_value);
}

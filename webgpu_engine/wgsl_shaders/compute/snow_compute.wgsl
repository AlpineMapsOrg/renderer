/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "util/tile_util.wgsl"
#include "util/tile_hashmap.wgsl"
#include "util/normals_util.wgsl"
#include "util/snow.wgsl"

struct SnowSettings {
    angle: vec4f,
    alt: vec4f,
}

struct RegionBounds {
    aabb_min: vec2f,
    aabb_max: vec2f,
}

// input
@group(0) @binding(0) var<uniform> snow_settings: SnowSettings;
@group(0) @binding(1) var<uniform> bounds: RegionBounds;
@group(0) @binding(2) var normal_texture: texture_2d<f32>;
@group(0) @binding(3) var height_texture: texture_2d<f32>;

// output
@group(0) @binding(4) var snow_texture: texture_storage_2d<rgba8unorm, write>; // ASSERT: same dimensions as heights_texture

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    // id.xy in [0, ceil(texture_dimensions(snow_texture).xy / workgroup_size.xy) - 1]

    // exit if thread id is outside image dimensions (i.e. thread is not supposed to be doing any work)
    let texture_size = textureDimensions(snow_texture);
    if (id.x >= texture_size.x || id.y >= texture_size.y) {
        return;
    }
    // get texture pos and uv
    let col = id.x; // in [0, texture_dimension(output_tiles).x - 1]
    let row = id.y; // in [0, texture_dimension(output_tiles).y - 1]
    let texture_pos = vec2u(col, row);
    let uv = vec2f(f32(col), f32(row)) / vec2f(texture_size - 1);
    
    // calculate width and height of input height texture in world space 
    let bounds_width: f32 = bounds.aabb_max.x - bounds.aabb_min.x;
    let bounds_height: f32 = bounds.aabb_max.y - bounds.aabb_min.y;

    // calculate x and y world positions
    let pos_x = bounds.aabb_min.x + bounds_width * uv.x;
    let pos_y = bounds.aabb_min.y + bounds_height * uv.y;

    // read normal and height (z world position)
    let normal: vec3f = textureLoad(normal_texture, texture_pos, 0).xyz;
    let altitude_correction_factor = 1 / cos(y_to_lat(pos_y)); //TODO currently, height decode node does no altitude correciton, so we need to account for that here 
    let pos_z: f32 = altitude_correction_factor * textureLoad(height_texture, texture_pos, 0).x;
    
    // compute snow and store in texture
    let snow = overlay_snow(normal, vec3f(pos_x, pos_y, pos_z), snow_settings.angle, snow_settings.alt);
    textureStore(snow_texture, texture_pos, snow);
}

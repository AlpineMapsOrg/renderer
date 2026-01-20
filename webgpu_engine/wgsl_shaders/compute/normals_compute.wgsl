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

#include "util/tile_util.wgsl"
#include "util/normals_util.wgsl"

// input
@group(0) @binding(0) var<uniform> bounds: RegionBounds;
@group(0) @binding(1) var heights_texture: texture_2d<f32>;

// output
@group(0) @binding(2) var normals_texture: texture_storage_2d<rgba8unorm, write>; // ASSERT: same dimensions as heights_texture

struct RegionBounds {
    aabb_min: vec2f,
    aabb_max: vec2f,
}

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    // id.xy in [0, ceil(texture_dimensions(normals_texture).xy / workgroup_size.xy) - 1]

    // exit if thread id is outside image dimensions (i.e. thread is not supposed to be doing any work)
    let texture_size = textureDimensions(normals_texture);
    if (id.x >= texture_size.x || id.y >= texture_size.y) {
        return;
    }
    let texture_pos: vec2u = id.xy; // in [0, texture_dimensions(normals_texture) - 1]

    // calculate width and height of input height texture in world space 
    let bounds_width: f32 = bounds.aabb_max.x - bounds.aabb_min.x;
    let bounds_height: f32 = bounds.aabb_max.y - bounds.aabb_min.y;

    // calculate width and height of a single input texel (quad)
    let quad_width: f32 = bounds_width / f32(texture_size.x - 1);
    let quad_height: f32 = bounds_height / f32(texture_size.y - 1);

    // calculate altitude correction factor based on latitude
    let pos_y = bounds.aabb_min.y + (bounds.aabb_max.y - bounds.aabb_min.y) * f32(id.y) / f32(texture_size.y);
    let altitude_correction_factor = 1 / cos(y_to_lat(pos_y));

    // calculate normal for position texture_pos from height texture and store in normal texture
    let normal = normal_by_finite_difference_method_texture_f32(texture_pos, quad_width, quad_height, altitude_correction_factor, heights_texture);
    textureStore(normals_texture, texture_pos, vec4f(0.5 * normal + 0.5, 1));
}

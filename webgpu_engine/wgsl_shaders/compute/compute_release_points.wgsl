/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2025 Markus Rampp
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "util/normals_util.wgsl"
#include "util/color_mapping.wgsl"

#define USE_ROUGHNESS_FILTERING 1
#define ROUGHNESS_THRESHOLD 0.01


// input
@group(0) @binding(0) var<uniform> settings: ReleasePointSettings;
@group(0) @binding(1) var normals_texture: texture_2d<f32>;

// output
//currently, format r8uint cannot be used for storage texture with write access (apparently only 32 bit formats can be used)
//TODO use storage buffer instead for now!
//  ASAP! saves 3 byte per texel (75%) immediately, could optimize further (just 3 bit per texel for current impl)
@group(0) @binding(2) var release_points_texture: texture_storage_2d<rgba8unorm, write>; // ASSERT: same dimensions as heights_texture

struct ReleasePointSettings {
    min_slope_angle: f32, // in rad
    max_slope_angle: f32, // in rad
    sampling_interval: vec2u,
}

fn should_paint(pos: vec2u) -> bool {
    return (pos.x % settings.sampling_interval.x == 0) && (pos.y % settings.sampling_interval.y == 0);
}

#if USE_ROUGHNESS_FILTERING 1
fn get_roughness(id: vec2u) -> f32 {
    // according to doi:10.5194/nhess-16-2211-2016
    if (id.x == 0 || id.y == 0 || id.x >= textureDimensions(normals_texture).x - 1 || id.y >= textureDimensions(normals_texture).y - 1) {
        return 1.0; // no roughness at borders
    }
    var idx = 0u;
    var r: array<vec3f, 9>;
    var r_sum = vec3f(0.0, 0.0, 0.0);

    for (var y = -1; y <= 1; y = y + 1) {
        for (var x = -1; x <= 1; x = x + 1) {
            let normal = textureLoad(normals_texture, vec2<i32>(id) + vec2(x, y), 0).xyz * 2 - 1;
            let alpha = acos(normal.z);             // slope in rad
            let beta = atan2(normal.x, normal.y);   // aspect in rad
            r[idx].x = sin(alpha) * cos(beta);      // x component of roughness vector
            r[idx].y = sin(alpha) * sin(beta);      // y component of roughness vector
            r[idx].z = cos(alpha);                  // z component of roughness vector
            idx = idx + 1u;
        }
    }
    for (var i = 0u; i < 9u; i = i + 1u) {
        r_sum = r_sum + r[i];
    }
    let r_magnitude = length(r_sum);
    let roughness = 1 - r_magnitude / 9.0;
    return roughness; // returns 0 for flat terrain, 1 for very rough terrain
}
#endif

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    // id.xy in [0, ceil(texture_dimensions(normals_texture).xy / workgroup_size.xy) - 1]

    // exit if thread id is outside image dimensions (i.e. thread is not supposed to be doing any work)
    let texture_size = textureDimensions(release_points_texture);
    if (id.x >= texture_size.x || id.y >= texture_size.y) {
        return;
    }
    // id.xy in [0, texture_dimensions(output_tiles) - 1]

    let tex_pos = id.xy;
    let normal = textureLoad(normals_texture, tex_pos, 0).xyz * 2 - 1;
    let slope_angle = get_slope_angle(normal); // slope angle in rad (0 flat, pi/2 vertical)

#if USE_ROUGHNESS_FILTERING 1
    let roughness = get_roughness(tex_pos);

    if (slope_angle < settings.min_slope_angle || slope_angle > settings.max_slope_angle || !should_paint(tex_pos) || roughness > ROUGHNESS_THRESHOLD) {
#else
    if (slope_angle < settings.min_slope_angle || slope_angle > settings.max_slope_angle || !should_paint(tex_pos)) {
#endif
        textureStore(release_points_texture, tex_pos, vec4f(0, 0, 0, 0));
    } else {
        let color = color_mapping_bergfex(slope_angle / (PI / 2));
        textureStore(release_points_texture, tex_pos, vec4f(color.xyz, 1));
        //textureStore(release_points_texture, tex_pos, vec4f(1, 0, 0, 1));
    }
}

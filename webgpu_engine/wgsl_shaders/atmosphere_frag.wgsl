/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2022 Adam Celarek
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

#include "camera_config.wgsl"
#include "atmosphere_implementation.wgsl"
#include "screen_pass_shared.wgsl"

@group(0) @binding(0) var<uniform> camera : camera_config;

// const highp float infinity = 1.0 / 0.0;   // gives a warning on webassembly (and other angle based products)
const infinity = 3.40282e+38; // https://godbolt.org/z/9o9PdbGqW

fn unproject(normalised_device_coordinates: vec2f) -> vec3f {
    let unprojected = camera.inv_proj_matrix * vec4(normalised_device_coordinates, 1.0, 1.0);
    let normalised_unprojected = unprojected / unprojected.w;
    return normalize((camera.inv_view_matrix * normalised_unprojected).xyz);
}

@fragment
fn fragmentMain(vertex_out : VertexOut) -> @location(0) vec4f {

    let origin = camera.position.xyz;
    let ray_direction = unproject(vertex_out.texcoords * vec2f(2.0, -2.0) + 1.0);
    var ray_length = 2000.0;
    let background_colour = vec3f(0.0, 0.0, 0.0);
    if (ray_direction.z < 0.0) {
        ray_length = min(ray_length, -(origin.z * 0.001) / ray_direction.z);
    }
    let light_through_atmosphere = calculate_atmospheric_light(origin / 1000.0, ray_direction, ray_length, background_colour, 1000);

    return vec4f(light_through_atmosphere, 1.0);
}

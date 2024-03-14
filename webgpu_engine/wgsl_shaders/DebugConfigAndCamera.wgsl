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

#include "shared_config.wgsl"
#include "hashing.wgsl"
#include "camera_config.wgsl"

@group(0) @binding(0) var<uniform> config : shared_config;
@group(0) @binding(1) var<uniform> camera : camera_config;

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4f
{
    //adapted from https://webgpu.github.io/webgpu-samples/sample/rotatingCube/
    const POSITIONS = array(
        vec3f(1, -1, 1),
        vec3f(-1, -1, 1),
        vec3f(-1, -1, -1),
        vec3f(1, -1, -1),
        vec3f(1, -1, 1),
        vec3f(-1, -1, -1),

        vec3f(1, 1, 1),
        vec3f(1, -1, 1),
        vec3f(1, -1, -1),
        vec3f(1, 1, -1),
        vec3f(1, 1, 1),
        vec3f(1, -1, -1),

        vec3f(-1, 1, 1),
        vec3f(1, 1, 1),
        vec3f(1, 1, -1),
        vec3f(-1, 1, -1),
        vec3f(-1, 1, 1),
        vec3f(1, 1, -1),

        vec3f(-1, -1, 1),
        vec3f(-1, 1, 1),
        vec3f(-1, 1, -1),
        vec3f(-1, -1, -1),
        vec3f(-1, -1, 1),
        vec3f(-1, 1, -1),

        vec3f(1, 1, 1),
        vec3f(-1, 1, 1),
        vec3f(-1, -1, 1),
        vec3f(-1, -1, 1),
        vec3f(1, -1, 1),
        vec3f(1, 1, 1),

        vec3f(1, -1, -1),
        vec3f(-1, -1, -1),
        vec3f(-1, 1, -1),
        vec3f(1, 1, -1),
        vec3f(1, -1, -1),
        vec3f(-1, 1, -1)
    );
    const cube_model = mat4x4f(
        10,   0,  0, 0,
        0,   10,  0, 0,
        0,    0, 10, 0,
        0,  -40,  0, 1
    );

    //const POSITIONS = array(vec3f(-10, -40, 0), vec3f(0, -40, 10), vec3f(10, -40, 0));
    let pos = vec4f(POSITIONS[vertex_index], 1);
    let clip_pos = camera.view_proj_matrix * cube_model * pos;
    return clip_pos;
}

@fragment
fn fragmentMain() -> @location(0) vec4f
{
    return vec4f(0.0, 0.4, (sin(config.sun_light_dir.x) + 1) / 2, 1.0);
}

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

@group(0) @binding(2) var height_texture : texture_2d<f32>;
@group(0) @binding(3) var height_sampler : sampler;


struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index : u32) -> VertexOut
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
    var vertex_out : VertexOut;
    vertex_out.position = clip_pos;
    vertex_out.uv = vec2f(pos.x / 2 + 0.5, pos.y / 2 + 0.5);
    return vertex_out;
}

@fragment fn fragmentMain(vertex_out : VertexOut)->@location(0) vec4f
{
    let color = textureSample(height_texture, height_sampler, vertex_out.uv).rgb;
    // return vec4f(color.r, 0.4, (sin(config.sun_light_dir.x) + 1) / 2, 1.0); //
    return vec4f(color, 1.0);
}

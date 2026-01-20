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

#include "util/shared_config.wgsl"
#include "util/camera_config.wgsl"

@group(0) @binding(0) var<uniform> config: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
@group(2) @binding(0) var depth_texture: texture_2d<f32>;
@group(3) @binding(0) var<storage> positions: array<vec4f>;
@group(3) @binding(1) var<uniform> line_config: LineConfig;

struct LineConfig {
    color: vec4f,
}

const behind_alpha = 0.35;

struct VertexOut {
    @builtin(position) position: vec4f,
}

struct FragIn {
    @builtin(position) position: vec4f,
}

struct FragOut {
    @location(0) color: vec4f,
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32) -> VertexOut {
    var vertex_out: VertexOut;
    let pos = positions[vertex_index];
    vertex_out.position = camera.view_proj_matrix * vec4f(pos.xyz - camera.position.xyz, 1);
    return vertex_out;
}

@fragment
fn fragmentMain(frag_in: FragIn) -> FragOut {
    var frag_out: FragOut;

    if (config.track_render_mode == 1) { // no depth test
        frag_out.color = line_config.color;
        return frag_out;
    }

    let depth_buffer_position = vec2u(frag_in.position.xy);
    let tile_fragment_depth = textureLoad(depth_texture, depth_buffer_position, 0).x;
    let line_fragment_depth = frag_in.position.z;

    if (tile_fragment_depth < line_fragment_depth) {
        if (config.track_render_mode == 2) { // depth test
            discard;
        } else if (config.track_render_mode == 3) { // semi-transparent if depth test failed
            frag_out.color = vec4f(line_config.color.xyz * behind_alpha * line_config.color.a, behind_alpha * line_config.color.a);
        }
    } else {
        frag_out.color = line_config.color;
    }

    return frag_out;
}
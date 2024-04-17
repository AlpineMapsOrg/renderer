/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

@group(1) @binding(0) var<uniform> camera : camera_config;

@group(2) @binding(0) var<uniform> n_edge_vertices : i32;
@group(2) @binding(1) var height_texture : texture_2d_array<u32>;
@group(2) @binding(2) var height_sampler : sampler;
@group(2) @binding(3) var ortho_texture : texture_2d_array<f32>;
@group(2) @binding(4) var ortho_sampler : sampler;

struct VertexIn {
    @location(0) bounds: vec4f,
    @location(1) texture_layer: i32,
    @location(2) tileset_id: i32,
    @location(3) tileset_zoomlevel: i32,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) pos_cws: vec3f,
    @location(2) normal: vec3f,
    @location(3) @interpolate(flat) texture_layer: i32,
    @location(4) color: vec3f,
}

struct FragOut {
    @location(0) albedo: vec4f,
    @location(1) position: vec4f,
    @location(2) normal: vec2u,
    @location(3) depth: vec4f,
}

fn y_to_lat(y: f32) -> f32 {
    const pi = 3.1415926535897932384626433;
    const cOriginShift = 20037508.342789244;

    let mercN = y * pi / cOriginShift;
    let latRad = 2.f * (atan(exp(mercN)) - (pi / 4.0));
    return latRad;
}

fn camera_world_space_position(vertex_index: u32, bounds: vec4f, texture_layer: i32, uv: ptr<function, vec2f>, n_quads_per_direction: ptr<function, f32>, quad_width: ptr<function, f32>,
    quad_height: ptr<function, f32>, altitude_correction_factor: ptr<function, f32>) -> vec3f
{
    let n_quads_per_direction_int = n_edge_vertices - 1;
    *n_quads_per_direction = f32(n_quads_per_direction_int);
    *quad_width = (bounds.z - bounds.x) / (*n_quads_per_direction);
    *quad_height = (bounds.w - bounds.y) / (*n_quads_per_direction);

    var row : i32 = i32(vertex_index) / n_edge_vertices;
    var col : i32 = i32(vertex_index) - (row * n_edge_vertices);
    let curtain_vertex_id = i32(vertex_index) - n_edge_vertices * n_edge_vertices;
    if (curtain_vertex_id >= 0) {
        if (curtain_vertex_id < n_edge_vertices) {
            row = (n_edge_vertices - 1) - curtain_vertex_id;
            col = (n_edge_vertices - 1);
        }
        else if (curtain_vertex_id >= n_edge_vertices && curtain_vertex_id < 2 * n_edge_vertices - 1) {
            row = 0;
            col = (n_edge_vertices - 1) - (curtain_vertex_id - n_edge_vertices) - 1;
        }
        else if (curtain_vertex_id >= 2 * n_edge_vertices - 1 && curtain_vertex_id < 3 * n_edge_vertices - 2) {
            row = curtain_vertex_id - 2 * n_edge_vertices + 2;
            col = 0;
        }
        else {
            row = (n_edge_vertices - 1);
            col = curtain_vertex_id - 3 * n_edge_vertices + 3;
        }
    }
    // Note: for higher zoom levels it would be enough to calculate the altitude_correction_factor on cpu
    // for lower zoom levels we could bake it into the texture.
    // but there was no measurable difference despite a cos and a atan, so leaving as is for now.
    let var_pos_cws_y : f32 = f32(n_quads_per_direction_int - row) * f32(*quad_width) + bounds.y;
    let pos_y : f32 = var_pos_cws_y + camera.position.y;
    *altitude_correction_factor = 0.125 / cos(y_to_lat(pos_y)); // https://github.com/AlpineMapsOrg/renderer/issues/5

    *uv = vec2f(f32(col) / (*n_quads_per_direction), f32(row) / (*n_quads_per_direction));
    let altitude_tex = f32(textureLoad(height_texture, vec2i(col, row), texture_layer, 0).r);
    let adjusted_altitude : f32 = altitude_tex * (*altitude_correction_factor);

    var var_pos_cws = vec3f(f32(col) * (*quad_width) + bounds.x, var_pos_cws_y, adjusted_altitude - camera.position.z);

    if (curtain_vertex_id >= 0) {
        // TODO implement preprocessor constants in shader
        //float curtain_height = CURTAIN_REFERENCE_HEIGHT;

        var curtain_height = f32(1000);
// TODO implement preprocessor if in shader
/*#if CURTAIN_HEIGHT_MODE == 1
        let dist_factor = clamp(length(var_pos_cws) / 100000.0, 0.2, 1.0);
        curtain_height *= dist_factor;
#endif*/
        var_pos_cws.z = var_pos_cws.z - curtain_height;
    }

    return var_pos_cws;
}

// TODO port
/*highp vec3 camera_world_space_position() {
    vec2 uv;
    float n_quads_per_direction;
    float quad_width;
    float quad_height;
    float altitude_correction_factor;
    return camera_world_space_position(uv, n_quads_per_direction, quad_width, quad_height, altitude_correction_factor);
}

highp vec3 normal_by_finite_difference_method(vec2 uv, float edge_vertices_count, float quad_width, float quad_height, float altitude_correction_factor) {
    // from here: https://stackoverflow.com/questions/6656358/calculating-normals-in-a-triangle-mesh/21660173#21660173
    vec2 offset = vec2(1.0, 0.0) / (edge_vertices_count);
    float height = quad_width + quad_height;
    highp float hL = float(texture(height_sampler, vec3(uv - offset.xy, texture_layer)).r);
    hL *= altitude_correction_factor;
    highp float hR = float(texture(height_sampler, vec3(uv + offset.xy, texture_layer)).r);
    hR *= altitude_correction_factor;
    highp float hD = float(texture(height_sampler, vec3(uv + offset.yx, texture_layer)).r);
    hD *= altitude_correction_factor;
    highp float hU = float(texture(height_sampler, vec3(uv - offset.yx, texture_layer)).r);
    hU *= altitude_correction_factor;

    return normalize(vec3(hL - hR, hD - hU, height));
}*/


@vertex fn vertexMain(@builtin(vertex_index) vertex_index : u32, vertex_in : VertexIn) -> VertexOut
{
    //adapted from https://webgpu.github.io/webgpu-samples/sample/rotatingCube/
    
    var uv : vec2f;
    var n_quads_per_direction : f32;
    var quad_width : f32;
    var quad_height : f32;
    var altitude_correction_factor : f32;
    let var_pos_cws = camera_world_space_position(vertex_index, vertex_in.bounds, vertex_in.texture_layer, &uv, &n_quads_per_direction, &quad_width, &quad_height, &altitude_correction_factor);

    let pos = vec4f(var_pos_cws, 1);
    let clip_pos = camera.view_proj_matrix * pos;

    var vertex_out : VertexOut;
    vertex_out.position = clip_pos;
    vertex_out.uv = uv;
    vertex_out.pos_cws = var_pos_cws;
    vertex_out.normal = vec3f(1.0, 1.0, 1.0); //TODO
    vertex_out.texture_layer = vertex_in.texture_layer;
    vertex_out.color = vec3f(1.0, 1.0, 1.0); //TODO
    return vertex_out;
}

@fragment fn fragmentMain(vertex_out : VertexOut) -> FragOut
{
    let albedo = textureSample(ortho_texture, ortho_sampler, vertex_out.uv, vertex_out.texture_layer).rgb;

    var frag_out : FragOut;
    frag_out.albedo = vec4f(albedo, 1.0);
    frag_out.position = vec4f(vertex_out.pos_cws, length(vertex_out.pos_cws));
    frag_out.normal = vec2u(0, 0);
    frag_out.depth = vec4f(0, 0, 0, 0);
    return frag_out;
}

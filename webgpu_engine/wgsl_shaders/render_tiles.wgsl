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

#include "util/shared_config.wgsl"
#include "util/hashing.wgsl"
#include "util/camera_config.wgsl"
#include "util/encoder.wgsl"
#include "util/tile_util.wgsl"
#include "util/normals_util.wgsl"
#include "util/snow.wgsl"
#include "util/filtering.wgsl"

@group(0) @binding(0) var<uniform> config: shared_config;

@group(1) @binding(0) var<uniform> camera: camera_config;

@group(2) @binding(0) var<uniform> n_edge_vertices: i32;
@group(2) @binding(1) var height_texture: texture_2d_array<u32>;
@group(2) @binding(2) var height_sampler: sampler;
@group(2) @binding(3) var ortho_texture: texture_2d_array<f32>;
@group(2) @binding(4) var ortho_sampler: sampler;

struct VertexIn {
    @location(0) bounds: vec4f,
    @location(1) height_texture_layer: i32,
    @location(2) ortho_texture_layer: i32,
    @location(3) tileset_id: i32,
    @location(4) height_zoomlevel: i32,
    @location(5) tile_id: vec4<u32>,
    @location(6) ortho_zoomlevel: i32,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) pos_cws: vec3f,
    @location(2) normal: vec3f,
    @location(3) @interpolate(flat) height_texture_layer: i32,
    @location(4) @interpolate(flat) ortho_texture_layer: i32,
    @location(5) @interpolate(flat) color: vec3f,
    @location(6) @interpolate(flat) tile_id: vec3<u32>,
    @location(7) @interpolate(flat) ortho_zoomlevel: i32,
}

struct FragOut {
    @location(0) albedo: u32,
    @location(1) position: vec4f,
    @location(2) normal_enc: vec2u,
    @location(3) overlay: u32,
}


fn compute_vertex(
    vertex_index: i32,
    render_tile_id: TileId,
    bounds: vec4f,
    height_zoomlevel: u32,
    height_texture_layer: i32,
    position: ptr<function, vec3f>,
    uv: ptr<function, vec2f>,
    tile_id: ptr<function, TileId>,
    compute_normal: bool,
    normal: ptr<function, vec3f>
) {
    // get tile id of desired height tile
    var height_tile_id: TileId;
    {
        const input_uv = vec2f(0.0);
        var height_uv: vec2f;
        decrease_zoom_level_until(render_tile_id, input_uv, height_zoomlevel, &height_tile_id, &height_uv);
    }

    let n_quads_per_direction_int: i32 = (n_edge_vertices - 1) >> (render_tile_id.zoomlevel - height_tile_id.zoomlevel);
    let n_quads_per_direction: f32 = f32(n_quads_per_direction_int);
    let quad_size: f32 = (bounds.z - bounds.x) / n_quads_per_direction;

    // get row and col for specific vertex_index
    var row: i32 = vertex_index / n_edge_vertices;
    var col: i32 = vertex_index - (row * n_edge_vertices);
    var curtain_vertex_id: i32 = vertex_index - n_edge_vertices * n_edge_vertices;
    if (curtain_vertex_id >= 0) {
        if (curtain_vertex_id < n_edge_vertices) {
            row = (n_edge_vertices - 1) - curtain_vertex_id;
            col = (n_edge_vertices - 1);
        } else if (curtain_vertex_id >= n_edge_vertices && curtain_vertex_id < 2 * n_edge_vertices - 1) {
            row = 0;
            col = (n_edge_vertices - 1) - (curtain_vertex_id - n_edge_vertices) - 1;
        } else if (curtain_vertex_id >= 2 * n_edge_vertices - 1 && curtain_vertex_id < 3 * n_edge_vertices - 2) {
            row = curtain_vertex_id - 2 * n_edge_vertices + 2;
            col = 0;
        } else {
            row = (n_edge_vertices - 1);
            col = curtain_vertex_id - 3 * n_edge_vertices + 3;
        }
    }
    if (row > n_quads_per_direction_int) {
        row = n_quads_per_direction_int;
        curtain_vertex_id = 1;
    }
    if (col > n_quads_per_direction_int) {
        col = n_quads_per_direction_int;
        curtain_vertex_id = 1;
    }

    // compute world space x and y coordinates
    (*position).y = f32(n_quads_per_direction_int - row) * f32(quad_size) + bounds.y;
    (*position).x = f32(col) * quad_size + bounds.x;
    
    // compute uv coordinates on height tile
    let render_tile_uv = vec2f(f32(col) / n_quads_per_direction, f32(row) / n_quads_per_direction);
    var height_tile_uv: vec2f;
    {
        var unused: TileId;
        decrease_zoom_level_until(render_tile_id, render_tile_uv, height_zoomlevel, &unused, &height_tile_uv);
    }
    *uv = render_tile_uv;

    // read height texture and compute world space z coordinate
    let altitude_tex = f32(bilinear_sample_u32(height_texture, height_sampler, height_tile_uv, u32(height_texture_layer)));
    // Note: for higher zoom levels it would be enough to calculate the altitude_correction_factor on cpu
    // for lower zoom levels we could bake it into the texture.
    // there was no measurable difference despite a cos and a atan, so leaving as is for now.
    let world_space_y: f32 = (*position).y + camera.position.y;
    let altitude_correction_factor: f32 = 0.125 / cos(y_to_lat(world_space_y)); // https://github.com/AlpineMapsOrg/renderer/issues/5
    let adjusted_altitude: f32 = altitude_tex * altitude_correction_factor;
    (*position).z = adjusted_altitude - camera.position.z;

    if (curtain_vertex_id >= 0) {
        const curtain_height = 1000.0;
        (*position).z = (*position).z - curtain_height;
    }

    //TODO port this
/*    if (curtain_vertex_id >= 0) {
        float curtain_height = CURTAIN_REFERENCE_HEIGHT;
if CURTAIN_HEIGHT_MODE == 1
        float dist_factor = clamp(length(position) / 100000.0, 0.2, 1.0);
        curtain_height *= dist_factor;
endif
if CURTAIN_HEIGHT_MODE == 2
        float zoom_factor = 1.0 - max(0.1, float(tile_id.z) / 25.f);
        curtain_height *= zoom_factor;
endif
        position.z = position.z - curtain_height;
    }
*/

    if (compute_normal) {
        *normal = normal_by_finite_difference_method(height_tile_uv, quad_size, quad_size, altitude_correction_factor, height_texture_layer, height_texture);
    }
}

fn normal_by_fragment_position_interpolation(pos_cws: vec3<f32>) -> vec3<f32> {
    let dFdxPos = dpdy(pos_cws);
    let dFdyPos = dpdx(pos_cws);
    return normalize(cross(dFdxPos, dFdyPos));
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32, vertex_in: VertexIn) -> VertexOut { 
    let render_tile_id = TileId(vertex_in.tile_id.x, vertex_in.tile_id.y, vertex_in.tile_id.z, 4294967295u);

    var position: vec3f;
    var uv: vec2f;
    var height_tile_id: TileId;
    var normal: vec3f;
    compute_vertex(i32(vertex_index), render_tile_id, vertex_in.bounds, u32(vertex_in.height_zoomlevel), vertex_in.height_texture_layer,
        &position, &uv, &height_tile_id, true, &normal);

    let clip_pos: vec4f = camera.view_proj_matrix * vec4f(position, 1.0);

    var vertex_out: VertexOut;
    vertex_out.position = clip_pos;
    vertex_out.uv = uv;
    vertex_out.pos_cws = position;
    vertex_out.normal = normal;
    vertex_out.height_texture_layer = vertex_in.height_texture_layer;
    vertex_out.ortho_texture_layer = vertex_in.ortho_texture_layer;

    var vertex_color = vec3f(0.0);
    if (config.overlay_mode == 2) {
        vertex_color = color_from_id_hash(u32(vertex_in.tileset_id));
    } else if (config.overlay_mode == 3) {
        vertex_color = color_from_id_hash(u32(vertex_in.height_zoomlevel));
        //vertex_color = color_from_id_hash(u32(vertex_in.ortho_zoomlevel));
    } else if (config.overlay_mode == 4) {
        vertex_color = color_from_id_hash(u32(vertex_index));
    }
    vertex_out.color = vertex_color;
    vertex_out.tile_id = vertex_in.tile_id.xyz;
    vertex_out.ortho_zoomlevel = vertex_in.ortho_zoomlevel;
    return vertex_out;
}

@fragment
fn fragmentMain(vertex_out: VertexOut) -> FragOut {
    let tile_id = TileId(vertex_out.tile_id.x, vertex_out.tile_id.y, vertex_out.tile_id.z, 4294967295u);

    // obtain uv coordinates for desired ortho zoom level and sample
    var ortho_tile_id: TileId;
    var ortho_uv: vec2f;
    let found_ortho = decrease_zoom_level_until(tile_id, vertex_out.uv, u32(vertex_out.ortho_zoomlevel), &ortho_tile_id, &ortho_uv);
    var albedo = textureSample(ortho_texture, ortho_sampler, ortho_uv, vertex_out.ortho_texture_layer).rgb;

    var frag_out: FragOut;

    var dist = length(vertex_out.pos_cws);
    var normal = vertex_out.normal;
    if (config.normal_mode != 0) {
        if (config.normal_mode == 1) {
            normal = normal_by_fragment_position_interpolation(vertex_out.pos_cws);
        }

        frag_out.normal_enc = octNormalEncode2u16(normal);
    }

    // HANDLE OVERLAYS (and mix it with the albedo color) THAT CAN JUST BE DONE IN THIS STAGE
    // NOTE: Performancewise its generally better to handle overlays in the compose step! (overdraw)
    var overlay_color = vec4f(0.0);
    if (config.overlay_mode > 0u && config.overlay_mode < 100u) {
        if (config.overlay_mode == 1) {
            overlay_color = vec4f(normal * 0.5 + 0.5, 1.0);
        } else {
            overlay_color = vec4f(vertex_out.color.xyz, 1);
        }
        //albedo = mix(albedo, overlay_color.xyz, config.overlay_strength * overlay_color.w);
    }
    frag_out.overlay = pack4x8unorm(overlay_color);
    frag_out.albedo = pack4x8unorm(vec4f(albedo, 1.0));

    frag_out.position = vec4f(vertex_out.pos_cws, dist);

    return frag_out;
}

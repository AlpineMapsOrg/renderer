/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

// Flow surface, pass 3 of 3 (AvalancheRenderer::draw): a snow blanket draped over the terrain,
// drawn into the G-buffer. One quad per render-grid cell (instance); each corner sits at the
// surface altitude of avalanche_surface_field.wgsl (terrain + splatted flow depth). The depth
// field is smooth, so its min_depth contour, where fragments are discarded, is a smooth outline
// at the render grid's resolution. Particles above the blanket (spray, the front) stay
// visible; those inside are hidden by the depth test.

///use util/shared_config
///use util/camera_config
///use webgpu::encoder
///use webgpu::tile_util
///use avalanche_surface_common

@group(0) @binding(0) var<uniform> config: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
@group(2) @binding(0) var<storage, read> field: array<f32>; // FIELD_STRIDE_F32 per render node
@group(2) @binding(1) var<uniform> surface: SurfaceConfig;

const SNOW_ALBEDO = vec3f(0.93, 0.95, 0.98);

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) pos_cws: vec3f,
    @location(1) normal: vec3f,
    @location(2) speed: f32, // [m/s]
    @location(3) depth: f32, // [m] flow depth
}

struct FragOut {
    @location(0) albedo: u32,
    @location(1) position: vec4f,
    @location(2) normal_enc: vec2u,
    @location(3) overlay: u32,
}

fn viridis(f: f32) -> vec3f {
    let stops = array<vec3f, 5>(vec3f(0.267, 0.005, 0.329), vec3f(0.230, 0.322, 0.546), vec3f(0.128, 0.567, 0.551),
        vec3f(0.369, 0.789, 0.383), vec3f(0.993, 0.906, 0.144));
    let s = clamp(f, 0.0, 1.0) * 4.0;
    let i = min(u32(s), 3u);
    return mix(stops[i], stops[i + 1u], s - f32(i));
}

fn clamped(node: vec2i) -> u32 {
    return node_index(clamp(node, vec2i(0), vec2i(surface.nodes) - vec2i(1)));
}

fn altitude(node: vec2i) -> f32 {
    return field[clamped(node) * FIELD_STRIDE_F32];
}

fn depth(node: vec2i) -> f32 {
    return field[clamped(node) * FIELD_STRIDE_F32 + 1u];
}

fn hidden() -> VertexOut {
    var out: VertexOut;
    out.position = vec4f(2.0, 2.0, 2.0, 1.0); // outside the clip volume
    return out;
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index: u32, @builtin(instance_index) instance: u32) -> VertexOut {
    let cells_x = surface.nodes.x - 1u;
    let cell = vec2i(i32(instance % cells_x), i32(instance / cells_x));
    // Almost every cell of the domain is empty. The depth inside a cell is interpolated from its
    // corners, so it reaches min_depth only if a corner does.
    let deepest = max(max(depth(cell), depth(cell + vec2i(1, 0))), max(depth(cell + vec2i(0, 1)), depth(cell + vec2i(1, 1))));
    if deepest < surface.min_depth {
        return hidden();
    }

    let node = cell + vec2i(i32(vertex_index & 1u), i32(vertex_index >> 1u));
    let index = clamped(node) * FIELD_STRIDE_F32;
    // Normal from the neighbouring surface altitudes (central differences), in metres.
    let dhdx = (altitude(node + vec2i(1, 0)) - altitude(node - vec2i(1, 0))) / (2.0 * surface.spacing);
    let dhdy = (altitude(node + vec2i(0, 1)) - altitude(node - vec2i(0, 1))) / (2.0 * surface.spacing);

    // Metres to camera-relative world, as for the particles: z is stretched by 1/cos(latitude)
    // like the terrain (render_tiles).
    let metres = node_metres(node);
    let m = surface.xy_from_metres;
    let xy = vec2f(m.x * metres.x + m.z * metres.y, m.y * metres.x + m.w * metres.y) + surface.offset_cws;
    let world_per_metre = 1.0 / cos(y_to_lat(xy.y + camera.position.y));
    let pos = vec3f(xy, field[index] * world_per_metre - camera.position.z);

    var out: VertexOut;
    out.position = camera.view_proj_matrix * vec4f(pos, 1.0);
    out.pos_cws = pos;
    out.normal = normalize(vec3f(-dhdx, -dhdy, 1.0));
    out.depth = field[index + 1u];
    out.speed = field[index + 2u];
    return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> FragOut {
    if in.depth < surface.min_depth {
        discard;
    }
    var albedo = SNOW_ALBEDO;
    if surface.colour_mode == 1u {
        albedo = viridis(in.speed / surface.speed_max);
    }
    var out: FragOut;
    out.albedo = pack4x8unorm(vec4f(albedo, 1.0));
    out.position = vec4f(in.pos_cws, length(in.pos_cws));
    out.normal_enc = octNormalEncode2u16(normalize(in.normal));
    out.overlay = 0u;
    return out;
}

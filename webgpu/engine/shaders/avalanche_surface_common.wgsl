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

// Shared by the two flow-surface shaders (AvalancheRenderer): the uniform and the per-column
// accumulator layout. Must match AvalancheRenderer::SurfaceConfig.

struct SurfaceConfig {
    xy_from_metres: vec4f, // 2x2 linear part, column-major: world.xy = (x, y) metres mapped
    offset_cws: vec2f, // world offset of metre (0, 0) minus the camera position
    domain_origin: vec2f, // [m] grid node (0, 0)
    columns: vec2u, // grid nodes in x, y
    dx: f32, // [m] node spacing
    radius: f32, // [m] particle radius: the surface lies this far above the top particle centre
    particle_count: u32,
    min_count: u32, // particles a column needs to count as covered
    colour_mode: u32, // 0 snow, 1 speed
    speed_max: f32, // [m/s]
    region_size: vec2f, // [m] the terrain's extent, for uv = metres / region_size
    height_dim: vec2u, // texels of the terrain height texture
}

const PARTICLE_STRIDE_VEC4: u32 = 8u; // struct Particle of mpm_common.wgsl as eight vec4s
const COLUMN_STRIDE_U32: u32 = 4u; // top, bottom (inverted), count, speed
const Z_OFFSET_CM: f32 = 100000.0; // altitudes down to -1000 m stay positive
const ALTITUDE_INVERT: u32 = 2147483648u; // bottom is stored as ALTITUDE_INVERT - altitude

fn column_index(node: vec2i) -> u32 {
    return u32(node.y) * surface.columns.x + u32(node.x);
}

fn encode_altitude(metres: f32) -> u32 {
    return u32(clamp(metres * 100.0 + Z_OFFSET_CM, 0.0, f32(ALTITUDE_INVERT - 1u)));
}

fn decode_altitude(encoded: u32) -> f32 {
    return (f32(encoded) - Z_OFFSET_CM) / 100.0;
}

fn node_metres(node: vec2i) -> vec2f {
    return surface.domain_origin + vec2f(node) * surface.dx;
}

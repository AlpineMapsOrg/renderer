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

// Shared by the flow-surface shaders (AvalancheRenderer): the uniform and the layouts of the
// splat accumulator and the field. Must match AvalancheRenderer::SurfaceConfig.
//
// The surface lives on a render grid `refine` times finer than the solver's: node (i, j) sits at
// domain_origin + (i, j) * spacing. The particles are splatted onto it with the solver's own
// quadratic B-spline, at the solver's node spacing (`kernel`), so the field is as smooth as what
// the solver resolves but drawn at a finer step.

struct SurfaceConfig {
    xy_from_metres: vec4f, // 2x2 linear part, column-major: world.xy = (x, y) metres mapped
    offset_cws: vec2f, // world offset of metre (0, 0) minus the camera position
    domain_origin: vec2f, // [m] render node (0, 0), the solver's grid node (0, 0)
    nodes: vec2u, // render nodes in x, y
    spacing: f32, // [m] render node spacing
    kernel: f32, // [m] B-spline width: the solver's node spacing
    particle_count: u32,
    colour_mode: u32, // 0 snow, 1 speed
    speed_max: f32, // [m/s]
    volume: f32, // [m^3] snow per particle: the release volume / particle count
    min_depth: f32, // [m] thinner snow is not drawn
    clearance: f32, // [m] particles higher above the terrain are not splatted (spray)
    region_size: vec2f, // [m] the terrain's extent, for uv = metres / region_size
    height_dim: vec2u, // texels of the terrain height texture
    radius: f32, // [m] drawn particle radius
    track: u32, // 1: draw the track, where the flow has been since the release
}

const PARTICLE_STRIDE_VEC4: u32 = 8u; // struct Particle of mpm_common.wgsl as eight vec4s
const ACCUM_STRIDE_U32: u32 = 3u; // B-spline weight, weight * speed, weight * height, fixed point
const WEIGHT_SCALE: f32 = 65536.0; // one particle adds at most 0.5625 * this per node
const SPEED_SCALE: f32 = 256.0; // [1 / (m/s)]
const HEIGHT_SCALE: f32 = 256.0; // [1 / m]
const FIELD_STRIDE_F32: u32 = 4u; // surface altitude [m], depth [m], speed [m/s], peak depth [m]
const TRACK_DEPTH: f32 = 0.3; // [m] the track layer: scoured, compacted snow where the flow has passed
// Written into the G-buffer overlay slot by the avalanche's fragments: alpha 0, so the tile-debug
// overlay shows nothing there, and the screen-space snow overlay (screen_space_snow.wgsl) skips
// these pixels instead of painting over the avalanche.
const AVALANCHE_MARKER: u32 = 0x00FFFFFFu;

fn node_index(node: vec2i) -> u32 {
    return u32(node.y) * surface.nodes.x + u32(node.x);
}

fn node_metres(node: vec2i) -> vec2f {
    return surface.domain_origin + vec2f(node) * surface.spacing;
}

// Quadratic B-spline of the solver (mpm_common.wgsl), distance in kernel widths.
fn bspline(distance: f32) -> f32 {
    let d = abs(distance);
    if d < 0.5 {
        return 0.75 - d * d;
    }
    if d < 1.5 {
        let t = 1.5 - d;
        return 0.5 * t * t;
    }
    return 0.0;
}

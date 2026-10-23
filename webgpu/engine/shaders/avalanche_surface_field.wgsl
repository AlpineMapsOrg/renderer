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

// Flow surface, pass 2 of 3 (AvalancheRenderer::prepare): per render node, from the splatted
// sums (avalanche_surface_splat.wgsl):
// - depth = sum of B-spline weights * particle volume / kernel^2: the snow volume per area (the
//   kernel integrates to kernel^2 over the plane), at the release density. It decides where the
//   blanket is drawn (min_depth).
// - the blanket's height above the terrain: a moving flow is looser than at release, so its
//   particles reach higher than that depth. Particles filling a layer evenly sit at half its
//   height on average, so 2 * the weighted mean height + the drawn radius covers them. Faded to
//   the volume depth towards the edge, so the blanket meets the ground there.
// - the weighted mean speed.

///use avalanche_surface_common
///use avalanche_surface_terrain

@group(0) @binding(0) var<storage, read> accum: array<u32>;
@group(0) @binding(1) var<storage, read_write> field: array<f32>; // FIELD_STRIDE_F32 per node
@group(0) @binding(2) var height_texture: texture_2d<f32>;
@group(0) @binding(3) var<uniform> surface: SurfaceConfig;

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    if any(id.xy >= surface.nodes) {
        return;
    }
    let node = vec2i(id.xy);
    let index = node_index(node);
    let weight = f32(accum[index * ACCUM_STRIDE_U32]) / WEIGHT_SCALE;
    let depth = weight * surface.volume / (surface.kernel * surface.kernel);
    var speed = 0.0;
    var cover = depth;
    if weight > 0.0 {
        speed = f32(accum[index * ACCUM_STRIDE_U32 + 1u]) / SPEED_SCALE / weight;
        let mean_height = f32(accum[index * ACCUM_STRIDE_U32 + 2u]) / HEIGHT_SCALE / weight;
        let edge = smoothstep(surface.min_depth, 4.0 * surface.min_depth, depth);
        cover = mix(depth, max(depth, 2.0 * mean_height + surface.radius), edge);
    }
    let base = index * FIELD_STRIDE_F32;
    field[base] = terrain_altitude(node_metres(node)) + cover;
    field[base + 1u] = depth;
    field[base + 2u] = speed;
}

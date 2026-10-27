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
// - the peak depth since the release (kept across frames, cleared on a reset): the track. Where
//   the flow has been, a thin layer stays, as in AvaFrame's peak flow thickness rasters.
// - with entrainment, the cover the flow can still pick up (the solver's column_stats.cover, which
//   entrain_column() in mpm_grid_update.wgsl erodes), as a depth between the solver's columns. The
//   solver's particles move on the bare terrain, not on the cover, so the surface takes the higher
//   of the two rather than their sum.

///use avalanche_surface_common
///use avalanche_surface_terrain

@group(0) @binding(0) var<storage, read> accum: array<u32>;
@group(0) @binding(1) var<storage, read_write> field: array<f32>; // FIELD_STRIDE_F32 per node
@group(0) @binding(2) var height_texture: texture_2d<f32>;
@group(0) @binding(3) var<uniform> surface: SurfaceConfig;
@group(0) @binding(4) var<storage, read_write> peak: array<f32>; // per node, persistent
@group(0) @binding(5) var<storage, read> column_stats: array<u32>; // struct ColumnStats (mpm_common.wgsl) per column

const COLUMN_STATS_STRIDE_U32: u32 = 6u;
const COLUMN_COVER: u32 = 4u; // [m^3 of cover] left on the column's cell

fn column_cover(column: vec2u) -> f32 {
    return bitcast<f32>(column_stats[(column.y * surface.columns.x + column.x) * COLUMN_STATS_STRIDE_U32 + COLUMN_COVER]);
}

// [m] vertical: the cover volume per cell area, bilinear between the solver's columns (nodes).
fn cover_depth(node: vec2i) -> f32 {
    if surface.cover == 0u {
        return 0.0;
    }
    let f = vec2f(node) * surface.spacing / surface.kernel;
    let last = surface.columns - 1u;
    let lo = min(vec2u(f), last);
    let hi = min(lo + 1u, last);
    let t = f - vec2f(lo);
    let bottom = mix(column_cover(lo), column_cover(vec2u(hi.x, lo.y)), t.x);
    let top = mix(column_cover(vec2u(lo.x, hi.y)), column_cover(hi), t.x);
    return max(mix(bottom, top, t.y), 0.0) / (surface.kernel * surface.kernel);
}

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
    let deepest = max(peak[index], depth);
    peak[index] = deepest;
    if surface.track != 0u && deepest >= surface.min_depth {
        cover = max(cover, TRACK_DEPTH * mix(0.5, 1.0, smoothstep(surface.min_depth, 3.0 * surface.min_depth, deepest)));
    }
    let layer = cover_depth(node);
    cover = max(cover, layer);
    let base = index * FIELD_STRIDE_F32;
    field[base] = terrain_altitude(node_metres(node)) + cover;
    field[base + 1u] = depth;
    field[base + 2u] = speed;
    field[base + 3u] = select(0.0, deepest, surface.track != 0u);
    field[base + 4u] = layer;
}

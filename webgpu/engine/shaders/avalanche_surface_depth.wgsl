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

// Flow surface, pass 2 of 3 (AvalancheRenderer::prepare): the flow depth per grid column,
// from the binned particles (avalanche_surface_bin.wgsl) and the solver's own terrain: the top
// of the snow (highest particle plus its radius) above the ground. A column counts only where the
// flow is dense: its 3x3 neighbourhood must hold at least min_count particles, so a stray pair
// of particles (spray, the thin tail) does not raise a patch of blanket - those stay particles.
// Written with the column's top speed for the speed colours.

///use avalanche_surface_common
///use avalanche_surface_terrain

@group(0) @binding(0) var<storage, read> columns: array<u32>;
@group(0) @binding(1) var<storage, read_write> depth: array<vec2f>; // (depth [m], speed [m/s])
@group(0) @binding(2) var height_texture: texture_2d<f32>;
@group(0) @binding(3) var<uniform> surface: SurfaceConfig;

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    if any(id.xy >= surface.columns) {
        return;
    }
    let node = vec2i(id.xy);
    let base = column_index(node) * COLUMN_STRIDE_U32;
    var neighbourhood = 0u;
    for (var dy = -1; dy <= 1; dy++) {
        for (var dx = -1; dx <= 1; dx++) {
            let neighbour = node + vec2i(dx, dy);
            if all(neighbour >= vec2i(0)) && all(neighbour < vec2i(surface.columns)) {
                neighbourhood += columns[column_index(neighbour) * COLUMN_STRIDE_U32 + 2u];
            }
        }
    }
    var result = vec2f(0.0);
    if columns[base + 2u] > 0u && neighbourhood >= surface.min_count {
        let top = decode_altitude(columns[base]) + surface.radius;
        result = vec2f(max(top - terrain_altitude(node_metres(node)), 0.0), f32(columns[base + 3u]) / 100.0);
    }
    depth[column_index(node)] = result;
}

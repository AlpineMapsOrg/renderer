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

// Flow surface, pass 1 of 2 (AvalancheRenderer::prepare): bins the MLS-MPM particles into the
// solver's grid columns. Per column, four u32 accumulated with atomics on a zeroed buffer:
//   [0] highest particle altitude, [1] lowest (stored inverted, so atomicMax finds it),
//   [2] particle count, [3] highest speed.
// Altitudes are centimetres shifted by Z_OFFSET_CM so they stay positive. The surface pass
// (render_avalanche_surface.wgsl) reads them back; avalanche_surface_common.wgsl has the layout.

///use avalanche_surface_common

@group(0) @binding(0) var<storage, read> particles: array<vec4f>;
@group(0) @binding(1) var<storage, read_write> columns: array<atomic<u32>>;
@group(0) @binding(2) var<uniform> surface: SurfaceConfig;

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    let index = id.x;
    if index >= surface.particle_count {
        return;
    }
    let p0 = particles[index * PARTICLE_STRIDE_VEC4]; // position, mass
    if p0.w <= 0.0 {
        return; // inactive
    }
    // Nearest grid node: the columns are the solver's nodes, at domain_origin + index * dx.
    let node = vec2i(round((p0.xy - surface.domain_origin) / surface.dx));
    if any(node < vec2i(0)) || any(node >= vec2i(surface.columns)) {
        return;
    }
    let base = column_index(node) * COLUMN_STRIDE_U32;
    let z = encode_altitude(p0.z);
    let speed = length(particles[index * PARTICLE_STRIDE_VEC4 + 1u].xyz);
    atomicMax(&columns[base], z);
    atomicMax(&columns[base + 1u], ALTITUDE_INVERT - z);
    atomicAdd(&columns[base + 2u], 1u);
    atomicMax(&columns[base + 3u], u32(min(speed * 100.0, 4.0e9)));
}

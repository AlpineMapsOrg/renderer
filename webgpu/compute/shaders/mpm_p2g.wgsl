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

///use mpm_common

// Stage 1 of the MPM step: scatter particle mass and momentum onto the background grid.
// Uses the MLS-MPM formulation of Hu 2018, where the internal force and the APIC
// affine momentum are folded into a single affine matrix applied to the node offset -
// this is what removes the explicit gradient term and makes the transfer cheap enough
// to run every substep on the GPU.
//
// The particles come in tile order (mpm_sort_*.wgsl): a workgroup's 256 consecutive sorted
// particles mostly share one SORT_TILE x SORT_TILE tile of columns. Their contributions to the
// nodes that tile's stencils reach - a WINDOW x WINDOW x WINDOW_LAYERS block of the band - are
// summed in workgroup memory, and each block node is written to the grid once per workgroup
// instead of once per particle. Particles of other tiles (where a bucket starts or ends inside
// the workgroup) and nodes above the window write to the grid directly, as before. The grid
// accumulates in fixed point, so the order of the additions does not change the result.
//
// References (sections, equations and pages as in the PDFs; full list in mpm-mls-doc/refs.md):
//   Hu 2018         Y. Hu et al., "A moving least squares material point method with displacement
//                   discontinuity and two-way rigid body coupling", ACM TOG 37(4), Art. 146, 2018.
//   Jiang 2015      C. Jiang et al., "The affine particle-in-cell method", ACM TOG 34(4), Art. 51,
//                   2015.

// Columns per axis the stencils of one sort tile reach (base .. base + 2), and the band layers
// held in workgroup memory: 6 x 6 x 16 nodes, 9 KiB with the four accumulators. Higher layers
// (grid_layers > 16) go to the grid directly.
const WINDOW: i32 = SORT_TILE + 2;
const WINDOW_LAYERS: i32 = 16;
const WINDOW_NODES: i32 = WINDOW * WINDOW * WINDOW_LAYERS;

var<workgroup> wg_mass: array<atomic<u32>, WINDOW_NODES>; // MASS_SCALE units, below 2^32 for 256 particles
var<workgroup> wg_momentum: array<atomic<i32>, 3 * WINDOW_NODES>; // MOMENTUM_SCALE
var<workgroup> wg_home: vec2i; // first column of the workgroup's sort tile

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>, @builtin(local_invocation_index) local_index: u32,
    @builtin(workgroup_id) group: vec3<u32>) {
    // The yield count is per substep: zeroed here, counted by this substep's G2P. Dispatches
    // in one compute pass are ordered, so the store lands before any of G2P's adds.
    if id.x == 0u {
        atomicStore(&state.plastic_particles, 0u);
    }
    // The home tile is the tile of the workgroup's first sorted particle. No early return
    // anywhere below: every invocation has to reach both barriers.
    if local_index == 0u {
        wg_home = vec2i(-0x40000000); // no window: a workgroup of inactive particles
        let first = group.x * 256u;
        if first < settings.num_particles {
            let p = particles[sorted_index(first)];
            if p.mass > 0.0 {
                wg_home = sort_tile_origin(compute_kernel(to_grid_space(p.position)).base.xy);
            }
        }
    }
    workgroupBarrier();
    let home = wg_home;
    if id.x < settings.num_particles {
        scatter_particle(sorted_index(id.x), home);
    }
    workgroupBarrier();
    for (var w = i32(local_index); w < WINDOW_NODES; w += 256) {
        flush_window_node(w, home);
    }
}

fn scatter_particle(index: u32, home: vec2i) {
    let p = particles[index];
    if p.mass <= 0.0 {
        return; // inactive particle
    }

    let grid_pos = to_grid_space(p.position);
    let k = compute_kernel(grid_pos);
    mark_tiles(k.base.xy);

    // MLS-MPM force term: -dt * V0 * (4 / dx^2) * (P F^T), Hu 2018 sec. 3.4, eq. 15-18 (p. 7):
    // eq. 15 gives M_p^-1 = 4 / dx^2 for the quadratic kernel, eq. 18 the force with
    // dPsi/dF F^T. Plus the APIC momentum m C (Jiang 2015 sec. 5.3, eq. 8, p. 6); folding both
    // into one affine matrix is Hu 2018 sec. 4, step (1), p. 8. P F^T was computed by the previous
    // G2P from the SVD of its return mapping (see mpm_material.wgsl), so no SVD here.
    let inv_dx = 1.0 / settings.dx;
    let stress_term = -settings.dt * settings.particle_volume * (4.0 * inv_dx * inv_dx) * particle_kirchhoff(p);
    let affine = stress_term + p.mass * particle_c(p);
    let layer_stride = i32(settings.grid_res.x * settings.grid_res.y);

    for (var i = 0; i < 3; i++) {
        for (var j = 0; j < 3; j++) {
            for (var l = 0; l < 3; l++) {
                let offset = vec3i(i, j, l);
                let node = k.base + offset;
                let slot = grid_slot(node);
                if slot < 0 {
                    continue;
                }

                let weight = kernel_weight(k, offset);
                let dpos = (vec3f(offset) - k.fx) * settings.dx;
                // dpos = x_i - x_p in metres, as the 4 / dx^2 above needs.
                let momentum = weight * (p.mass * p.velocity + affine * dpos);
                let units = u32(round(weight * p.mass * MASS_SCALE));
                let fixed = vec3i(to_fixed(momentum.x), to_fixed(momentum.y), to_fixed(momentum.z));

                let local = node.xy - home;
                let layer = slot / layer_stride;
                if all(local >= vec2i(0)) && all(local < vec2i(WINDOW)) && layer < WINDOW_LAYERS {
                    let w = (layer * WINDOW + local.y) * WINDOW + local.x;
                    atomicAdd(&wg_mass[w], units);
                    atomicAdd(&wg_momentum[3 * w], fixed.x);
                    atomicAdd(&wg_momentum[3 * w + 1], fixed.y);
                    atomicAdd(&wg_momentum[3 * w + 2], fixed.z);
                } else {
                    let cell = u32(slot);
                    add_node_mass_units(cell, units);
                    atomicAdd(&grid[cell].vx, fixed.x);
                    atomicAdd(&grid[cell].vy, fixed.y);
                    atomicAdd(&grid[cell].vz, fixed.z);
                }
            }
        }
    }
}

// Writes one window node's sums to the grid. Only nodes that received a contribution are
// non-zero, and those passed grid_slot() in scatter_particle, so the slot is valid.
fn flush_window_node(w: i32, home: vec2i) {
    let units = atomicLoad(&wg_mass[w]);
    let momentum = vec3i(atomicLoad(&wg_momentum[3 * w]), atomicLoad(&wg_momentum[3 * w + 1]), atomicLoad(&wg_momentum[3 * w + 2]));
    if units == 0u && all(momentum == vec3i(0)) {
        return;
    }
    let node_xy = home + vec2i(w % WINDOW, (w / WINDOW) % WINDOW);
    let layer = w / (WINDOW * WINDOW);
    let cell = u32(layer * i32(settings.grid_res.x * settings.grid_res.y) + i32(column_index(node_xy)));
    add_node_mass_units(cell, units);
    atomicAdd(&grid[cell].vx, momentum.x);
    atomicAdd(&grid[cell].vy, momentum.y);
    atomicAdd(&grid[cell].vz, momentum.z);
}

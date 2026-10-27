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
// References (sections, equations and pages as in the PDFs; full list in mpm-mls-doc/refs.md):
//   Hu 2018         Y. Hu et al., "A moving least squares material point method with displacement
//                   discontinuity and two-way rigid body coupling", ACM TOG 37(4), Art. 146, 2018.
//   Jiang 2015      C. Jiang et al., "The affine particle-in-cell method", ACM TOG 34(4), Art. 51,
//                   2015.

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let index = id.x;
    // The yield count is per substep: zeroed here, counted by this substep's G2P. Dispatches
    // in one compute pass are ordered, so the store lands before any of G2P's adds.
    if index == 0u {
        atomicStore(&state.plastic_particles, 0u);
    }
    if index >= settings.num_particles {
        return;
    }

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

    for (var i = 0; i < 3; i++) {
        for (var j = 0; j < 3; j++) {
            for (var l = 0; l < 3; l++) {
                let offset = vec3i(i, j, l);
                let slot = grid_slot(k.base + offset);
                if slot < 0 {
                    continue;
                }
                let cell = u32(slot);

                let weight = kernel_weight(k, offset);
                let dpos = (vec3f(offset) - k.fx) * settings.dx;
                // dpos = x_i - x_p in metres, as the 4 / dx^2 above needs.
                let momentum = weight * (p.mass * p.velocity + affine * dpos);

                add_node_mass(cell, weight * p.mass);
                atomicAdd(&grid[cell].vx, to_fixed(momentum.x));
                atomicAdd(&grid[cell].vy, to_fixed(momentum.y));
                atomicAdd(&grid[cell].vz, to_fixed(momentum.z));
            }
        }
    }
}

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
// Uses the MLS-MPM formulation of Hu et al. [3], where the internal force and the APIC
// affine momentum are folded into a single affine matrix applied to the node offset -
// this is what removes the explicit gradient term and makes the transfer cheap enough
// to run every substep on the GPU.

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let index = id.x;
    if index >= settings.num_particles {
        return;
    }

    let p = particles[index];
    if p.mass <= 0.0 {
        return; // inactive particle
    }

    let grid_pos = to_grid_space(p.position);
    let k = compute_kernel(grid_pos);

    let f_elastic = particle_f(p);
    let stress = snow_stress(f_elastic, p.jp);

    // MLS-MPM force term: -dt * V0 * (4 / dx^2) * (P F^T)
    let inv_dx = 1.0 / settings.dx;
    let stress_term = -settings.dt * p.volume * (4.0 * inv_dx * inv_dx) * stress;
    let affine = stress_term + p.mass * particle_c(p);

    for (var i = 0; i < 3; i++) {
        for (var j = 0; j < 3; j++) {
            for (var l = 0; l < 3; l++) {
                let offset = vec3i(i, j, l);
                let node = k.base + offset;
                if !is_inside_grid(node) {
                    continue;
                }

                let weight = kernel_weight(k, offset);
                let dpos = (vec3f(offset) - k.fx) * settings.dx;
                let momentum = weight * (p.mass * p.velocity + affine * dpos);

                let cell = grid_index(node);
                atomicAdd(&grid[cell].mass, to_fixed(weight * p.mass));
                atomicAdd(&grid[cell].vx, to_fixed(momentum.x));
                atomicAdd(&grid[cell].vy, to_fixed(momentum.y));
                atomicAdd(&grid[cell].vz, to_fixed(momentum.z));
            }
        }
    }
}

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

// Stages 3 and 4 of the MPM step, fused into one pass over the particles: gather the
// updated grid velocity (plus the APIC affine matrix C), evolve the deformation gradient
// through the snow plasticity model, then advect the particle.

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let index = id.x;
    if index >= settings.num_particles {
        return;
    }

    var p = particles[index];
    if p.mass <= 0.0 {
        return;
    }

    let grid_pos = to_grid_space(p.position);
    let k = compute_kernel(grid_pos);
    let inv_dx = 1.0 / settings.dx;

    var new_velocity = vec3f(0.0);
    var new_c = mat3x3f(vec3f(0.0), vec3f(0.0), vec3f(0.0));

    for (var i = 0; i < 3; i++) {
        for (var j = 0; j < 3; j++) {
            for (var l = 0; l < 3; l++) {
                let offset = vec3i(i, j, l);
                let node = k.base + offset;
                if !is_inside_grid(node) {
                    continue;
                }

                let cell = grid_index(node);
                let node_velocity
                    = vec3f(from_fixed(atomicLoad(&grid[cell].vx)), from_fixed(atomicLoad(&grid[cell].vy)), from_fixed(atomicLoad(&grid[cell].vz)));

                let weight = kernel_weight(k, offset);
                let dpos = vec3f(offset) - k.fx; // in grid units

                new_velocity += weight * node_velocity;
                new_c += 4.0 * inv_dx * weight * outer_product(node_velocity, dpos);
            }
        }
    }

    p.velocity = new_velocity;
    store_c(&p, new_c);

    // Elastic predictor, then return the deformation gradient to the admissible set.
    let f_trial = (identity3() + settings.dt * new_c) * particle_f(p);
    let plastic = apply_plasticity(f_trial, p.jp);
    store_f(&p, plastic.f_elastic);
    p.jp = plastic.jp;

    // Advection.
    p.position += settings.dt * p.velocity;

    // Particle level terrain collision - keeps snow from creeping through the surface
    // between grid nodes, which the node level condition alone cannot prevent.
    let surface = terrain_height(p.position.xy);
    if p.position.z < surface {
        p.position.z = surface;
        p.velocity = resolve_terrain_collision(p.velocity, terrain_normal(p.position.xy));
    }

    // Keep particles inside the simulation box so grid indexing stays in range.
    let margin = 2.0 * settings.dx;
    let min_xy = settings.domain_origin + vec2f(margin);
    let max_xy = settings.domain_origin + vec2f(settings.domain_size_xy - margin);
    let clamped_xy = clamp(p.position.xy, min_xy, max_xy);
    if clamped_xy.x != p.position.xy.x { p.velocity.x = 0.0; }
    if clamped_xy.y != p.position.xy.y { p.velocity.y = 0.0; }
    p.position = vec3f(clamped_xy, p.position.z);

    let base_altitude = domain_base_altitude();
    let max_altitude = base_altitude + (f32(settings.grid_res.z) * settings.dx) - margin;
    let clamped_z = clamp(p.position.z, base_altitude + margin, max_altitude);
    if clamped_z != p.position.z {
        p.velocity.z = 0.0;
        p.position.z = clamped_z;
    }

    particles[index] = p;

    atomicMax(&state.max_speed_mm, u32(clamp(length(p.velocity) * 1000.0, 0.0, 4.0e9)));
}

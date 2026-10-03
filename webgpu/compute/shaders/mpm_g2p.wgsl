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
///use mpm_material
///use mpm_friction

// Stages 3 and 4 of the MPM step, fused into one pass over the particles: gather the
// updated grid velocity (plus the APIC affine matrix C), evolve the deformation gradient
// through the snow plasticity model, then advect the particle.
//
// Also gathers the per-substep diagnostics (SimState in mpm_common): max speed, how many
// particles yielded, the path travelled, and contacts with the box's artificial walls. They
// are reduced in workgroup memory first, so each workgroup touches the global counters once
// instead of every particle hammering the same few addresses.

var<workgroup> wg_max_speed_mm: atomic<u32>;
var<workgroup> wg_yielded: atomic<u32>;
var<workgroup> wg_path: atomic<u32>; // SUM_PATH_SCALE units; 256 particles fit a u32
var<workgroup> wg_ceiling_contacts: atomic<u32>;
var<workgroup> wg_wall_contacts: atomic<u32>;

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>, @builtin(local_invocation_index) local_index: u32) {
    // No early return: the workgroup barrier below has to be reached by every invocation.
    let index = id.x;
    if index < settings.num_particles {
        update_particle(index);
    }

    workgroupBarrier();
    if local_index == 0u {
        atomicMax(&state.max_speed_mm, atomicLoad(&wg_max_speed_mm));
        let yielded = atomicLoad(&wg_yielded);
        if yielded > 0u {
            atomicAdd(&state.plastic_particles, yielded);
        }
        let path = atomicLoad(&wg_path);
        if path > 0u && atomicAdd(&state.sum_path_lo, path) > 0xFFFFFFFFu - path {
            atomicAdd(&state.sum_path_hi, 1u);
        }
        let ceiling = atomicLoad(&wg_ceiling_contacts);
        if ceiling > 0u {
            atomicAdd(&state.ceiling_contacts, ceiling);
        }
        let walls = atomicLoad(&wg_wall_contacts);
        if walls > 0u {
            atomicAdd(&state.wall_contacts, walls);
        }
    }
}

fn update_particle(index: u32) {
    var p = particles[index];
    if p.mass <= 0.0 {
        return;
    }
    let old_xy = p.position.xy;

    let grid_pos = to_grid_space(p.position);
    let k = compute_kernel(grid_pos);
    let inv_dx = 1.0 / settings.dx;

    // Does the stencil reach the nodes where mpm_grid_update stops outward flow - the two
    // outer node rows of the box, the top three layers of the band? Then the box, not the
    // physics, is shaping this particle's motion.
    let res = vec3i(settings.grid_res);
    let wall_contact = any(k.base.xy < vec2i(2)) || any(k.base.xy + vec2i(2) >= res.xy - vec2i(3));
    let centre_column = column_index(clamp(k.base.xy + vec2i(1), vec2i(0), res.xy - vec2i(1)));
    let ceiling_contact = k.base.z + 2 - column_floor[centre_column] >= res.z - 3;

    var new_velocity = vec3f(0.0);
    var new_c = mat3x3f(vec3f(0.0), vec3f(0.0), vec3f(0.0));

    for (var i = 0; i < 3; i++) {
        for (var j = 0; j < 3; j++) {
            for (var l = 0; l < 3; l++) {
                let offset = vec3i(i, j, l);
                let slot = grid_slot(k.base + offset);
                if slot < 0 {
                    continue;
                }
                let cell = u32(slot);

                let node_velocity = grid_velocity[cell].xyz;

                let weight = kernel_weight(k, offset);
                let dpos = vec3f(offset) - k.fx; // in grid units

                new_velocity += weight * node_velocity;
                new_c += 4.0 * inv_dx * weight * outer_product(node_velocity, dpos);
            }
        }
    }

    p.velocity = new_velocity;
    store_c(&p, new_c);

    // Elastic predictor, then return the deformation gradient to the admissible set. The
    // return mapping also yields the stress of the new state, which the next P2G consumes.
    let f_trial = (identity3() + settings.dt * new_c) * particle_f(p);
    let plastic = material_plasticity(f_trial, p.plastic_state);
    store_f(&p, plastic.f_elastic);
    p.plastic_state = plastic.plastic_state;
    store_kirchhoff(&p, plastic.kirchhoff);
    if plastic.yielded {
        atomicAdd(&wg_yielded, 1u);
    }

    // Advection.
    p.position += settings.dt * p.velocity;

    // Particle level terrain collision - keeps snow from creeping through the surface
    // between grid nodes, which the node level condition alone cannot prevent.
    let surface = terrain_height(p.position.xy);
    if p.position.z < surface {
        p.position.z = surface;
        p.velocity = resolve_terrain_collision(p.velocity, terrain_normal(p.position.xy), false, 0.0);
    }

    // Keep particles inside the simulation box so grid indexing stays in range.
    let margin = 2.0 * settings.dx;
    let min_xy = settings.domain_origin + vec2f(margin);
    let max_xy = settings.domain_origin + vec2f(settings.domain_size_xy - margin);
    let clamped_xy = clamp(p.position.xy, min_xy, max_xy);
    if clamped_xy.x != p.position.xy.x { p.velocity.x = 0.0; }
    if clamped_xy.y != p.position.xy.y { p.velocity.y = 0.0; }
    if wall_contact || any(clamped_xy != p.position.xy) {
        atomicAdd(&wg_wall_contacts, 1u);
    }
    p.position = vec3f(clamped_xy, p.position.z);

    // Ceiling of the column's band: keep the stencil (1.5 cells) inside the stored layers.
    let column = column_index(vec2i(floor((p.position.xy - settings.domain_origin) / settings.dx)));
    let max_altitude = (f32(column_floor[column] + i32(settings.grid_res.z)) - 2.5) * settings.dx;
    var at_ceiling = ceiling_contact;
    if p.position.z > max_altitude {
        p.velocity.z = 0.0;
        p.position.z = max_altitude;
        at_ceiling = true;
    }
    if at_ceiling {
        atomicAdd(&wg_ceiling_contacts, 1u);
    }

    particles[index] = p;

    atomicMax(&wg_max_speed_mm, u32(clamp(length(p.velocity) * 1000.0, 0.0, 4.0e9)));
    // Rounded, not truncated, so the sum carries no systematic bias; below 0.005 mm per
    // substep (well under 1 mm/s) a particle counts as resting. Capped at 100 m per substep,
    // which only a diverging particle reaches, so 256 of them still fit the u32.
    atomicAdd(&wg_path, u32(round(min(length(p.position.xy - old_xy), 100.0) * SUM_PATH_SCALE)));
}

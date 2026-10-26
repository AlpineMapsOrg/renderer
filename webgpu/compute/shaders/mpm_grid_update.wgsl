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
///use mpm_friction
///use mpm_material
///use random

// Stage 2 of the MPM step: turn accumulated momentum into velocity, apply gravity and
// resolve boundary conditions (terrain and domain walls). The velocity goes to grid_velocity
// as plain floats for G2P; the fixed-point accumulators are zeroed on the way out, because
// this pass is their last reader - the next P2G finds a clean grid without a clear pass.
//
// One workgroup per TILE_SIZE x TILE_SIZE tile of columns, each thread walks its column's
// layers. Tiles that no particle's stencil reached this substep (P2G flags them, see
// mark_tiles() in mpm_common) return straight away: their accumulators are still zero and no
// particle will read their velocities.

var<workgroup> tile_active: u32;

// Returns the node's mass, which the column sums into its peak depth, and mass times speed.
fn update_node(node_xy: vec2i, layer: i32, cell: u32, flow_depth: f32) -> vec2f {
    let mass_lo = atomicLoad(&grid[cell].mass_lo);
    let mass_hi = atomicLoad(&grid[cell].mass_hi);
    let momentum_fixed = vec3i(atomicLoad(&grid[cell].vx), atomicLoad(&grid[cell].vy), atomicLoad(&grid[cell].vz));

    let untouched = mass_lo == 0u && mass_hi == 0u && all(momentum_fixed == vec3i(0));
    if !untouched {
        atomicStore(&grid[cell].mass_lo, 0u);
        atomicStore(&grid[cell].mass_hi, 0u);
        atomicStore(&grid[cell].vx, 0);
        atomicStore(&grid[cell].vy, 0);
        atomicStore(&grid[cell].vz, 0);
    }

    let mass = (f32(mass_hi) * 4294967296.0 + f32(mass_lo)) / MASS_SCALE;
    if mass <= 0.0 {
        // Empty node - make sure G2P never gathers stale velocity from it.
        grid_velocity[cell] = vec4f(0.0);
        return vec2f(0.0);
    }

    let momentum = vec3f(momentum_fixed) / MOMENTUM_SCALE;
    var velocity = momentum / mass;
    velocity.z -= settings.gravity * settings.dt; // z is altitude

    // Terrain boundary: nodes below the surface push the material back out.
    let world = column_node_world(node_xy, layer);
    let surface = terrain_height(world.xy);
    if world.z < surface {
        velocity = resolve_terrain_collision(velocity, terrain_normal(world.xy), true, flow_depth);
    }

    // Domain walls: no outflow through the sides of the box or the top of the band. The
    // bottom of the band lies inside the terrain, which the condition above handles.
    let res = vec3i(settings.grid_res);
    if node_xy.x < 2 && velocity.x < 0.0 { velocity.x = 0.0; }
    if node_xy.x >= res.x - 3 && velocity.x > 0.0 { velocity.x = 0.0; }
    if node_xy.y < 2 && velocity.y < 0.0 { velocity.y = 0.0; }
    if node_xy.y >= res.y - 3 && velocity.y > 0.0 { velocity.y = 0.0; }
    if layer >= res.z - 3 && velocity.z > 0.0 { velocity.z = 0.0; }

    grid_velocity[cell] = vec4f(velocity, 0.0);
    return vec2f(mass, mass * length(velocity));
}

// Entrainment (com1DFA, Tonnel et al. 2023), per column like com1DFA's per cell: the flow over the
// column takes snow from its cover (column_stats.cover, set at the reset by mpm_prepare).
//   Plowing: the front takes the whole cover, as soon as the flow is plowing_depth deep.
//   Erosion: the flow erodes tau_b |u| / e_b of cover mass per area and second, tau_b the basal
//            shear stress of the solver's friction law (Coulomb mu rho g h cos(slope), plus
//            Voellmy's rho g |u|^2 / xi) - com1DFA's default mode.
// Eroded cover becomes particles from the pool after the release particles, each with the mass
// of a release particle (cover volume V rho_snow / rho_cover each, V the real volume per release
// particle), placed at rest on the ground of the column's cell. They are written with negative
// mass, which keeps them out of this substep's G2P gather: G2P only sets them to mass at rest
// (see mpm_g2p), and the next P2G adds that mass without momentum - so the flow slows by exactly
// the momentum the snow takes up, com1DFA's -u rho dV/dt term, from the grid alone.
const MAX_SPAWN_PER_SUBSTEP: u32 = 64u; // per column; the rest waits in `eroded`
fn entrain_column(node_xy: vec2i, column: u32, flow_depth: f32, speed: f32) {
    let stats = &column_stats[column];
    var cover = (*stats).cover;
    if (cover <= 0.0 && (*stats).eroded <= 0.0) || flow_depth <= 0.0 || speed < settings.entrainment_min_speed {
        return;
    }
    let world_xy = settings.domain_origin + vec2f(node_xy) * settings.dx;
    var eroded = 0.0;
    if settings.entrainment_mode == ENTRAINMENT_PLOWING {
        if flow_depth >= settings.plowing_depth {
            eroded = cover;
        }
    } else {
        var tau = settings.terrain_friction * settings.snow_density * settings.gravity * flow_depth * terrain_normal(world_xy).z;
        if settings.basal_friction_model == FRICTION_VOELLMY {
            tau += settings.snow_density * settings.gravity * speed * speed / settings.voellmy_xi;
        }
        let rate = tau * speed / settings.erosion_energy; // [kg/m^2/s]
        eroded = min(cover, rate * settings.dt * settings.dx * settings.dx / settings.entrainment_density);
    }
    cover -= eroded;
    var pending = (*stats).eroded + eroded;

    let per_particle = seeded_particle_volume() / settings.cover_density_ratio; // [m^3 of cover]
    var count = min(u32(floor(pending / per_particle)), MAX_SPAWN_PER_SUBSTEP);
    for (var n = 0u; n < count; n++) {
        let slot = atomicAdd(&state.entrained_particles, 1u);
        if slot >= settings.pool_particles {
            // Pool used up: the snow stays on the ground.
            atomicAdd(&state.pool_exhausted, u32(round(pending * COVER_VOLUME_SCALE)));
            cover += pending;
            pending = 0.0;
            break;
        }
        seed(vec4u(column, slot, settings.random_seed, 0x5bd1e995u));
        let xy = world_xy + (rand2() - vec2f(0.5)) * settings.dx;
        var p: Particle;
        p.position = vec3f(xy, terrain_height(xy) + rand() * 0.5);
        p.mass = -settings.particle_mass; // entrained this substep, see above
        p.velocity = vec3f(0.0);
        p.plastic_state = material_initial_state();
        store_c(&p, mat3x3f(vec3f(0.0), vec3f(0.0), vec3f(0.0)));
        store_f(&p, identity3());
        store_kirchhoff(&p, mat3x3f(vec3f(0.0), vec3f(0.0), vec3f(0.0)));
        particles[settings.release_particles + slot] = p;
        pending -= per_particle;
    }
    (*stats).cover = cover;
    (*stats).eroded = pending;
}

@compute @workgroup_size(8, 8, 1) // TILE_SIZE x TILE_SIZE
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>, @builtin(workgroup_id) group: vec3<u32>,
    @builtin(local_invocation_index) local_index: u32) {
    let tile = group.y * tiles_x() + group.x;
    if local_index == 0u {
        tile_active = atomicLoad(&tile_flags[tile]);
    }
    // Barrier included: every thread has the flag before thread 0 resets it below.
    if workgroupUniformLoad(&tile_active) == 0u {
        return;
    }
    if local_index == 0u {
        atomicStore(&tile_flags[tile], 0u); // the next P2G flags it again if still needed
    }

    if any(id.xy >= settings.grid_res.xy) {
        return;
    }
    let node_xy = vec2i(id.xy);
    let column = column_index(node_xy);
    let layer_stride = settings.grid_res.x * settings.grid_res.y;

    // Local flow depth for the Voellmy drag: the snow volume above this column per unit
    // area. The column's node masses sum the particles' B-spline weights over x and y only
    // (the z weights add up to 1), so this is a smoothed areal mass, turned into a depth with
    // the real volume per particle. Only Voellmy needs it, and the branch is uniform.
    var flow_depth = 0.0;
    if settings.basal_friction_model == FRICTION_VOELLMY {
        var column_mass = 0.0;
        for (var layer = 0u; layer < settings.grid_res.z; layer++) {
            column_mass += node_mass(layer * layer_stride + column);
        }
        flow_depth = column_mass / settings.particle_mass * seeded_particle_volume() / (settings.dx * settings.dx);
    }

    var column_mass = 0.0;
    var column_momentum = 0.0; // sum of mass * speed
    for (var layer = 0u; layer < settings.grid_res.z; layer++) {
        let node = update_node(node_xy, i32(layer), layer * layer_stride + column, flow_depth);
        column_mass += node.x;
        column_momentum += node.y;
    }
    if settings.entrainment_mode != ENTRAINMENT_OFF && column_mass > 0.0 {
        let depth = column_mass / settings.particle_mass * seeded_particle_volume() / (settings.dx * settings.dx);
        entrain_column(node_xy, column, depth, column_momentum / column_mass);
    }

    // Peaks of the column (see ColumnStats). Depth from the column's mass as for Voellmy above.
    // Speed from the particles, not the grid: in MLS-MPM a node's momentum holds the stress
    // impulse too, which at the edge of the flow, on nodes with almost no mass, gives grid
    // speeds of hundreds of m/s that no particle has.
    let stats = &column_stats[column];
    if column_mass > 0.0 {
        let depth = column_mass / settings.particle_mass * seeded_particle_volume() / (settings.dx * settings.dx);
        (*stats).peak_depth = max((*stats).peak_depth, depth);
    }
    let particles = atomicLoad(&(*stats).particles);
    if particles > 0u {
        let speed = f32(atomicLoad(&(*stats).speed_sum)) / SPEED_SUM_SCALE / f32(particles);
        (*stats).peak_speed = max((*stats).peak_speed, speed);
        atomicStore(&(*stats).particles, 0u);
        atomicStore(&(*stats).speed_sum, 0u);
    }
}

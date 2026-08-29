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
///use random

// Places one particle per thread inside the release area that overlaps the simulation
// domain, forming a snow slab of settings.slab_thickness on top of the terrain.
// Particles that fail to find a release point stay inactive (mass = 0) and are skipped
// by every later stage.

const MAX_SEED_ATTEMPTS: u32 = 32u;

fn is_release_point(uv: vec2f) -> bool {
    let dim = textureDimensions(release_point_texture);
    let texel = vec2u(clamp(uv, vec2f(0.0), vec2f(0.9999)) * vec2f(dim));
    return textureLoad(release_point_texture, texel, 0).a > 0.0;
}

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let index = id.x;
    if index >= settings.num_particles {
        return;
    }

    seed(vec4u(index, settings.random_seed, 0x9e3779b9u, 0x85ebca6bu));

    var p: Particle;
    p.mass = 0.0;
    p.volume = settings.particle_volume;
    p.velocity = vec3f(0.0);
    p.jp = 1.0;
    p.position = vec3f(0.0);
    store_c(&p, mat3x3f(vec3f(0.0), vec3f(0.0), vec3f(0.0)));
    store_f(&p, identity3());

    // Particles start in the release disc, not across the whole domain - the start zone is
    // small and the domain is large so the avalanche has somewhere to run out to.
    let release_centre = vec2f(settings.release_centre_x, settings.release_centre_y);
    let domain_min = settings.domain_origin + vec2f(2.0 * settings.dx);
    let domain_max = settings.domain_origin + vec2f(settings.domain_size_xy - 2.0 * settings.dx);

    var found = false;
    var world_xy = vec2f(0.0);
    for (var attempt = 0u; attempt < MAX_SEED_ATTEMPTS; attempt++) {
        // sqrt on the radius keeps the samples uniform over the disc area.
        let r = rand2();
        let angle = r.x * 6.28318530718;
        let distance = sqrt(r.y) * settings.release_radius;
        let candidate = release_centre + vec2f(cos(angle), sin(angle)) * distance;

        // A particle outside the grid box would be clamped onto its wall immediately.
        if any(candidate < domain_min) || any(candidate > domain_max) {
            continue;
        }
        if settings.seed_anywhere > 0.5 || is_release_point(world_to_uv(candidate)) {
            found = true;
            world_xy = candidate;
            break;
        }
    }

    if found {
        // Stack particles through the slab so the release volume has real thickness.
        let depth = rand() * settings.slab_thickness;
        p.position = vec3f(world_xy, terrain_height(world_xy) + depth);
        p.mass = settings.particle_mass;
        atomicAdd(&state.active_particles, 1u);
    }

    particles[index] = p;
}

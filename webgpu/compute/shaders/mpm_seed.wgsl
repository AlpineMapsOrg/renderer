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
///use random

// Places one particle per thread inside the release area that overlaps the simulation
// domain, forming a snow slab on top of the terrain: settings.slab_thickness deep in a disc
// on the map, the release depth of each cell in the sandbox (RELEASE_GRID).
// Particles that fail to find a release point stay inactive (mass = 0) and are skipped
// by every later stage. So do the slots after the release particles, the pool entrainment
// draws from (see entrain_column() in mpm_grid_update).
//
// References (sections, equations and pages as in the PDFs; full list in mpm-mls-doc/refs.md):
//   Stomakhin 2013  A. Stomakhin et al., "A material point method for snow simulation",
//                   ACM TOG 32(4), Art. 102, 2013.
//   Klar 2016       G. Klar et al., "Drucker-Prager elastoplasticity for sand animation",
//                   ACM TOG 35(4), Art. 103, 2016.
//   Gaume 2018      J. Gaume et al., "Dynamic anticrack propagation in snow", Nat. Commun. 9,
//                   3047, 2018.
//   Li 2021         X. Li et al., "Three-dimensional and real-scale modeling of flow regimes in
//                   dense snow avalanches", Landslides 18, 2021.
//   Tonnel 2023     M. Tonnel et al., "AvaFrame com1DFA (v1.3): a thickness-integrated
//                   computational avalanche module - theory, numerics, and testing",
//                   Geosci. Model Dev. 16, 7013-7035, 2023.

const MAX_SEED_ATTEMPTS: u32 = 32u;
// The sandbox's release cells can fill only a small share of their bounding box (avaMal's
// three separate areas, a narrow diagonal gully) - more attempts keep the misses rare.
const MAX_GRID_SEED_ATTEMPTS: u32 = 256u;

fn release_texel(uv: vec2f) -> vec4f {
    let dim = textureDimensions(release_point_texture);
    let texel = vec2u(clamp(uv, vec2f(0.0), vec2f(0.9999)) * vec2f(dim));
    return textureLoad(release_point_texture, texel, 0);
}

fn is_release_point(uv: vec2f) -> bool { return release_texel(uv).a > 0.0; }

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let index = id.x;
    if index >= settings.num_particles {
        return;
    }

    seed(vec4u(index, settings.random_seed, 0x9e3779b9u, 0x85ebca6bu));

    var p: Particle;
    p.mass = 0.0;
    p.velocity = vec3f(0.0);
    p.plastic_state = material_initial_state();
    p.position = vec3f(0.0);
    store_c(&p, mat3x3f(vec3f(0.0), vec3f(0.0), vec3f(0.0)));
    store_f(&p, identity3());
    // F = I is stress free in every model. The initial plastic states: Stomakhin 2013 sec. 7
    // (F_P = I, so J_P = 1, p. 6), Klar 2016 sec. 7.3 eq. 29 (q = 0, p. 9), and Gaume 2018 eq. 4
    // (p. 2) inverted for the alpha that gives the initial consolidation pressure.
    store_kirchhoff(&p, mat3x3f(vec3f(0.0), vec3f(0.0), vec3f(0.0)));

    // Particles start in the release disc, not across the whole domain - the start zone is
    // small and the domain is large so the avalanche has somewhere to run out to.
    let release_centre = vec2f(settings.release_centre_x, settings.release_centre_y);
    let domain_min = settings.domain_origin + vec2f(2.0 * settings.dx);
    let domain_max = settings.domain_origin + vec2f(settings.domain_size_xy - 2.0 * settings.dx);

    var found = false;
    var world_xy = vec2f(0.0);
    var slab_depth = settings.slab_thickness;
    if index >= settings.release_particles {
        particles[index] = p; // a pool slot: inactive until entrainment fills it
        return;
    }
    if settings.release_mode == RELEASE_GRID {
        // AvaFrame release thickness per cell (as com1DFA reads it, Tonnel 2023).
        // Rejection sampling over the bounds of the release cells: a sample is kept with
        // probability depth / max depth, so particles per area follow the release depth and
        // every particle stands for the same volume (seeded_particle_volume()).
        let half = vec2f(settings.release_radius, settings.release_half_y);
        for (var attempt = 0u; attempt < MAX_GRID_SEED_ATTEMPTS; attempt++) {
            let candidate = release_centre + (rand2() * 2.0 - vec2f(1.0)) * half;
            if any(candidate < domain_min) || any(candidate > domain_max) {
                continue;
            }
            let depth = release_texel(world_to_uv(candidate)).r;
            if depth > 0.0 && rand() * settings.release_max_depth < depth {
                found = true;
                world_xy = candidate;
                slab_depth = depth;
                break;
            }
        }
    }
    for (var attempt = 0u; attempt < MAX_SEED_ATTEMPTS && settings.release_mode == RELEASE_DISC; attempt++) {
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
            // The first sample is uniform over the disc, so the share of threads whose first
            // sample hits is the share of the disc that holds snow - see seeded_particle_volume().
            if attempt == 0u {
                atomicAdd(&state.seed_first_hits, 1u);
            }
            found = true;
            world_xy = candidate;
            break;
        }
    }

    if found {
        // Stack particles through the slab so the release volume has real thickness. Like Li 2021
        // (simulation setup, p. 4: a 1.05 m slab in the release zone only), snow starts in the
        // release area alone.
        let depth = rand() * slab_depth;
        p.position = vec3f(world_xy, terrain_height(world_xy) + depth);
        p.mass = settings.particle_mass;
        atomicAdd(&state.active_particles, 1u);
    }

    particles[index] = p;
}

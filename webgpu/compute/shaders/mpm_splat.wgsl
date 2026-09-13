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

// Accumulates the particle cloud into a top-down density raster covering the simulation
// domain. This is the 2D projection used for the map overlay and for export; the raster
// is also what a future 3D particle renderer would replace.

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let index = id.x;
    if index >= settings.num_particles {
        return;
    }

    let p = particles[index];
    if p.mass <= 0.0 {
        return;
    }

    // Li et al. 2021 report the plastic particle ratio per flow regime (77 / 26 / 10 / 34 %
    // for their cases I-IV); counting it here, once per run, makes that a cheap sanity metric.
    if abs(p.plastic_state - material_initial_state()) > 1e-6 {
        atomicAdd(&state.plastic_particles, 1u);
    }

    // Centre of mass and mean kinetic energy, see SimState in mpm_common. 64-bit sums as
    // lo/hi u32 pairs: the carry is detected from the value atomicAdd hands back.
    let sx = u32(max(p.position.x, 0.0) * SUM_POSITION_SCALE);
    if atomicAdd(&state.sum_x_lo, sx) > 0xFFFFFFFFu - sx { atomicAdd(&state.sum_x_hi, 1u); }
    let sy = u32(max(p.position.y, 0.0) * SUM_POSITION_SCALE);
    if atomicAdd(&state.sum_y_lo, sy) > 0xFFFFFFFFu - sy { atomicAdd(&state.sum_y_hi, 1u); }
    let sz = u32(max(p.position.z, 0.0) * SUM_POSITION_SCALE);
    if atomicAdd(&state.sum_z_lo, sz) > 0xFFFFFFFFu - sz { atomicAdd(&state.sum_z_hi, 1u); }
    let sv = u32(min(dot(p.velocity, p.velocity) * SUM_SPEED_SQ_SCALE, 4.0e9));
    if atomicAdd(&state.sum_speed_sq_lo, sv) > 0xFFFFFFFFu - sv { atomicAdd(&state.sum_speed_sq_hi, 1u); }

    // Domain-local position in [0,1]^2, with v flipped to match the texture convention.
    let local = (p.position.xy - settings.domain_origin) / settings.domain_size_xy;
    if any(local < vec2f(0.0)) || any(local >= vec2f(1.0)) {
        return;
    }

    let centre = vec2f(local.x, 1.0 - local.y) * vec2f(settings.raster_dim);

    // A particle stands for a parcel of snow, not a point - splat it as a disc, otherwise
    // the result is a scatter of single texels and invisible at map scale.
    let radius = settings.splat_radius_texels;
    let extent = i32(ceil(radius));
    let radius_sq = radius * radius;
    let dim = vec2i(settings.raster_dim);

    for (var dy = -extent; dy <= extent; dy++) {
        for (var dx = -extent; dx <= extent; dx++) {
            if f32(dx * dx + dy * dy) > radius_sq {
                continue;
            }
            let texel = vec2i(centre) + vec2i(dx, dy);
            if any(texel < vec2i(0)) || any(texel >= dim) {
                continue;
            }
            let raster_index = u32(texel.y * dim.x + texel.x);
            atomicAdd(&density_raster[raster_index], 1u);
        }
    }
}

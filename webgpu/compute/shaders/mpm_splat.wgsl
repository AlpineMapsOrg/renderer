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

    // Domain-local position in [0,1]^2, with v flipped to match the texture convention.
    let local = (p.position.xy - settings.domain_origin) / settings.domain_size_xy;
    if any(local < vec2f(0.0)) || any(local >= vec2f(1.0)) {
        return;
    }

    let texel = vec2u(vec2f(local.x, 1.0 - local.y) * vec2f(settings.raster_dim));
    let clamped = min(texel, settings.raster_dim - vec2u(1));
    let raster_index = clamped.y * settings.raster_dim.x + clamped.x;

    atomicAdd(&density_raster[raster_index], 1u);
}

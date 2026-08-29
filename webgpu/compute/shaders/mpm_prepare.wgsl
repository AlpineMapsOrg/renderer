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

// Scans the terrain inside the simulation domain to establish the vertical origin of the
// grid. Runs once per reset, before any particles are seeded. state.min/max_altitude_cm
// are pre-seeded from the CPU with extreme values so the atomics converge correctly.

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    if id.x >= settings.grid_res.x || id.y >= settings.grid_res.y {
        return;
    }

    let world_xy = settings.domain_origin + (vec2f(id.xy) + vec2f(0.5)) * settings.dx;
    let altitude_cm = i32(terrain_height(world_xy) * 100.0);

    atomicMin(&state.min_altitude_cm, altitude_cm);
    atomicMax(&state.max_altitude_cm, altitude_cm);
}

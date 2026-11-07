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

// Particle sort, pass 1 of 3: how many particles each sort tile holds (see "Particle indices
// sorted by sort tile" in mpm_common). The counts start at zero: the buffer is cleared at a
// reset, and mpm_sort_scan zeroes them again after reading them.

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    if id.x >= settings.num_particles {
        return;
    }
    atomicAdd(&tile_flags[sort_counts_offset() + sort_bucket(particles[id.x])], 1u);
}

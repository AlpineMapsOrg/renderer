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

// Particle sort, pass 3 of 3: every particle takes the next position of its bucket. The order
// within a bucket follows the scheduling of this pass; P2G does not depend on it.

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    if id.x >= settings.num_particles {
        return;
    }
    let position = atomicAdd(&tile_flags[sort_cursors_offset() + sort_bucket(particles[id.x])], 1u);
    atomicStore(&tile_flags[sorted_indices_offset() + position], id.x);
}

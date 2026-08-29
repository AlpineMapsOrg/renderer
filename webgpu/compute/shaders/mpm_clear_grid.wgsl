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

// Resets the background grid at the start of every MPM substep. This is a compute kernel
// rather than a buffer clear so that a whole run - all substeps, the splat and the
// rasterisation - fits into a single compute pass.

@compute @workgroup_size(256, 1, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    let node_count = settings.grid_res.x * settings.grid_res.y * settings.grid_res.z;
    if id.x >= node_count {
        return;
    }

    atomicStore(&grid[id.x].mass, 0);
    atomicStore(&grid[id.x].vx, 0);
    atomicStore(&grid[id.x].vy, 0);
    atomicStore(&grid[id.x].vz, 0);
}

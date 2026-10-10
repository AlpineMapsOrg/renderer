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

// Sums the particles' speed per grid column for the peak velocity raster (ColumnStats in
// mpm_common). Runs after G2P, only when the peaks are wanted (the AvaFrame sandbox); the next
// substep's grid update turns the sums into the column's mean speed and zeroes them.
//
// A pass of its own rather than two more atomics in P2G: those are invisible to the physics,
// yet they changed how the compiler arranged P2G's float arithmetic, and the trace drifted by
// millimetres. Kept separate, the diagnostics cannot touch the solver.

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
    // The column of the nearest node. It lies inside the particle's stencil, so the next P2G
    // marks its tile active and the grid update visits it.
    let grid_pos = to_grid_space(p.position);
    let own = clamp(vec2i(round(grid_pos.xy)), vec2i(0), vec2i(settings.grid_res.xy) - vec2i(1));
    let column = column_index(own);
    atomicAdd(&column_stats[column].particles, 1u);
    atomicAdd(&column_stats[column].speed_sum, u32(min(length(p.velocity), 1000.0) * SPEED_SUM_SCALE));
}

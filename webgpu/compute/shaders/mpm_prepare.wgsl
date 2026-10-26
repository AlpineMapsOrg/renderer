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

// Scans the terrain inside the simulation domain once per reset, before any particles
// are seeded: the altitude range (for the readout) and, per grid column, the floor of the
// stored band of layers - see grid_slot() in mpm_common. state.min/max_altitude_cm are
// pre-seeded from the CPU with extreme values so the atomics converge correctly.
//
// With entrainment, also the snow cover of each column's cell (the dx x dx square around the
// node), 4 x 4 samples of cover_depth_at(): what the flow can pick up there (column_stats.cover).

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    if id.x >= settings.grid_res.x || id.y >= settings.grid_res.y {
        return;
    }

    // Sample at the node itself - the terrain condition in mpm_grid_update does the same.
    let world_xy = settings.domain_origin + vec2f(id.xy) * settings.dx;
    let altitude = terrain_height(world_xy);
    let altitude_cm = i32(altitude * 100.0);

    atomicMin(&state.min_altitude_cm, altitude_cm);
    atomicMax(&state.max_altitude_cm, altitude_cm);

    column_floor[column_index(vec2i(id.xy))] = i32(floor(altitude / settings.dx)) - COLUMN_FLOOR_MARGIN;

    if settings.entrainment_mode == ENTRAINMENT_OFF || settings.cover_mode == COVER_NONE {
        return;
    }
    var cover = 0.0; // [m^3]
    for (var i = 0u; i < 4u; i++) {
        for (var j = 0u; j < 4u; j++) {
            let sample_xy = world_xy + (vec2f(f32(i), f32(j)) + vec2f(0.5) - vec2f(2.0)) * 0.25 * settings.dx;
            if !under_release(sample_xy) {
                cover += cover_depth_at(sample_xy);
            }
        }
    }
    cover *= settings.dx * settings.dx / 16.0;
    column_stats[column_index(vec2i(id.xy))].cover = cover;
    if cover > 0.0 {
        atomicAdd(&state.cover_volume, u32(round(cover * COVER_VOLUME_SCALE)));
    }
}

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
///use mpm_friction

// Stage 2 of the MPM step: turn accumulated momentum into velocity, apply gravity and
// resolve boundary conditions (terrain and domain walls). The velocity goes to grid_velocity
// as plain floats for G2P; the fixed-point accumulators are zeroed on the way out, because
// this pass is their last reader - the next P2G finds a clean grid without a clear pass.
//
// One workgroup per TILE_SIZE x TILE_SIZE tile of columns, each thread walks its column's
// layers. Tiles that no particle's stencil reached this substep (P2G flags them, see
// mark_tiles() in mpm_common) return straight away: their accumulators are still zero and no
// particle will read their velocities.

var<workgroup> tile_active: u32;

fn update_node(node_xy: vec2i, layer: i32, cell: u32) {
    let mass_lo = atomicLoad(&grid[cell].mass_lo);
    let mass_hi = atomicLoad(&grid[cell].mass_hi);
    let momentum_fixed = vec3i(atomicLoad(&grid[cell].vx), atomicLoad(&grid[cell].vy), atomicLoad(&grid[cell].vz));

    let untouched = mass_lo == 0u && mass_hi == 0u && all(momentum_fixed == vec3i(0));
    if !untouched {
        atomicStore(&grid[cell].mass_lo, 0u);
        atomicStore(&grid[cell].mass_hi, 0u);
        atomicStore(&grid[cell].vx, 0);
        atomicStore(&grid[cell].vy, 0);
        atomicStore(&grid[cell].vz, 0);
    }

    let mass = (f32(mass_hi) * 4294967296.0 + f32(mass_lo)) / MASS_SCALE;
    if mass <= 0.0 {
        // Empty node - make sure G2P never gathers stale velocity from it.
        grid_velocity[cell] = vec4f(0.0);
        return;
    }

    let momentum = vec3f(momentum_fixed) / MOMENTUM_SCALE;
    var velocity = momentum / mass;
    velocity.z -= settings.gravity * settings.dt; // z is altitude

    // Terrain boundary: nodes below the surface push the material back out.
    let world = column_node_world(node_xy, layer);
    let surface = terrain_height(world.xy);
    if world.z < surface {
        velocity = resolve_terrain_collision(velocity, terrain_normal(world.xy), true);
    }

    // Domain walls: no outflow through the sides of the box or the top of the band. The
    // bottom of the band lies inside the terrain, which the condition above handles.
    let res = vec3i(settings.grid_res);
    if node_xy.x < 2 && velocity.x < 0.0 { velocity.x = 0.0; }
    if node_xy.x >= res.x - 3 && velocity.x > 0.0 { velocity.x = 0.0; }
    if node_xy.y < 2 && velocity.y < 0.0 { velocity.y = 0.0; }
    if node_xy.y >= res.y - 3 && velocity.y > 0.0 { velocity.y = 0.0; }
    if layer >= res.z - 3 && velocity.z > 0.0 { velocity.z = 0.0; }

    grid_velocity[cell] = vec4f(velocity, 0.0);
}

@compute @workgroup_size(8, 8, 1) // TILE_SIZE x TILE_SIZE
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>, @builtin(workgroup_id) group: vec3<u32>,
    @builtin(local_invocation_index) local_index: u32) {
    let tile = group.y * tiles_x() + group.x;
    if local_index == 0u {
        tile_active = atomicLoad(&tile_flags[tile]);
    }
    // Barrier included: every thread has the flag before thread 0 resets it below.
    if workgroupUniformLoad(&tile_active) == 0u {
        return;
    }
    if local_index == 0u {
        atomicStore(&tile_flags[tile], 0u); // the next P2G flags it again if still needed
    }

    if any(id.xy >= settings.grid_res.xy) {
        return;
    }
    let node_xy = vec2i(id.xy);
    let column = column_index(node_xy);
    let layer_stride = settings.grid_res.x * settings.grid_res.y;
    for (var layer = 0u; layer < settings.grid_res.z; layer++) {
        update_node(node_xy, i32(layer), layer * layer_stride + column);
    }
}

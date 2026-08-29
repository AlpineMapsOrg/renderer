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

// Stage 2 of the MPM step: turn accumulated momentum into velocity, apply gravity and
// resolve boundary conditions (terrain and domain walls). The velocity is written back
// into the momentum slots, so the G2P stage reads these nodes as plain velocities.

@compute @workgroup_size(4, 4, 4)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    if any(id >= settings.grid_res) {
        return;
    }

    let node = vec3i(id);
    let cell = grid_index(node);

    let mass = from_fixed(atomicLoad(&grid[cell].mass));
    if mass <= 1e-9 {
        // Empty node - make sure G2P never gathers stale velocity from it.
        atomicStore(&grid[cell].vx, 0);
        atomicStore(&grid[cell].vy, 0);
        atomicStore(&grid[cell].vz, 0);
        return;
    }

    let momentum = vec3f(from_fixed(atomicLoad(&grid[cell].vx)), from_fixed(atomicLoad(&grid[cell].vy)), from_fixed(atomicLoad(&grid[cell].vz)));

    var velocity = momentum / mass;
    velocity.z -= settings.gravity * settings.dt; // z is altitude

    // Terrain boundary: nodes below the surface push the material back out.
    let world = to_world_space(vec3f(node));
    let surface = terrain_height(world.xy);
    if world.z < surface {
        velocity = resolve_terrain_collision(velocity, terrain_normal(world.xy));
    }

    // Domain walls: no outflow through the sides, floor or ceiling of the grid box.
    let res = vec3i(settings.grid_res);
    if node.x < 2 && velocity.x < 0.0 { velocity.x = 0.0; }
    if node.x >= res.x - 3 && velocity.x > 0.0 { velocity.x = 0.0; }
    if node.y < 2 && velocity.y < 0.0 { velocity.y = 0.0; }
    if node.y >= res.y - 3 && velocity.y > 0.0 { velocity.y = 0.0; }
    if node.z < 2 && velocity.z < 0.0 { velocity.z = 0.0; }
    if node.z >= res.z - 3 && velocity.z > 0.0 { velocity.z = 0.0; }

    atomicStore(&grid[cell].vx, to_fixed(velocity.x));
    atomicStore(&grid[cell].vy, to_fixed(velocity.y));
    atomicStore(&grid[cell].vz, to_fixed(velocity.z));
}

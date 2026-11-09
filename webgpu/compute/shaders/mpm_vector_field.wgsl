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

///use vector_field

// The baseline vector field of the MLS-MPM solver (MpmBaselineFieldBuilder): the grid velocity
// averaged over depth, one record per column of the terrain-following grid.
//
// Runs on its own bind group, not the solver's: the solver's bindings already use all eight
// storage buffers a stage may have, and this pass needs an output buffer. It reads only the grid
// velocity, whose fourth component is the node's mass (written by mpm_grid_update). That is the
// whole interface to the solver - the band layout enters only as "layers of a column are
// grid_res.x * grid_res.y nodes apart", so the terrain floor of a column does not matter for an
// average over its layers.
//
// Per column:  mass = sum m,  velocity = sum m v / sum m  (mass-weighted mean over depth),
//              kg = mass * kg_per_mass_unit,  depth = kg / (snow_density dx^2).
//
// A node at the edge of the flow with almost no mass can carry a large velocity (its momentum
// includes the stress impulse of the particles), and a mass-weighted mean over a column made of
// such nodes alone would show it. Columns with less than min_mass in total are empty instead.

struct Params {
    grid_res: vec3u, // nodes in x and y, layers per column
    dx: f32, // [m]
    kg_per_mass_unit: f32, // [kg] one unit of node mass stands for; 0 while the particle volume is unknown
    snow_density: f32, // [kg/m^3]
    min_mass: f32, // node mass units below which a column counts as empty
    unused: f32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> grid_velocity: array<vec4f>; // xyz velocity, w node mass
@group(0) @binding(2) var<storage, read_write> field: array<FieldRecord>; // one per column

@compute @workgroup_size(8, 8, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    if any(id.xy >= params.grid_res.xy) {
        return;
    }
    let column = id.y * params.grid_res.x + id.x;
    let layer_stride = params.grid_res.x * params.grid_res.y;

    var mass = 0.0;
    var momentum = vec3f(0.0);
    for (var layer = 0u; layer < params.grid_res.z; layer++) {
        let node = grid_velocity[layer * layer_stride + column];
        mass += node.w;
        momentum += node.w * node.xyz;
    }

    var record: FieldRecord; // zero: an empty cell
    if mass >= params.min_mass && mass > 0.0 {
        record.velocity = momentum / mass;
        record.flags = FIELD_VELOCITY;
        if params.kg_per_mass_unit > 0.0 {
            let kg = mass * params.kg_per_mass_unit;
            record.mass = kg;
            record.depth = kg / (params.snow_density * params.dx * params.dx);
            record.flags |= FIELD_DEPTH | FIELD_MASS;
        }
    }
    field[column] = record;
}

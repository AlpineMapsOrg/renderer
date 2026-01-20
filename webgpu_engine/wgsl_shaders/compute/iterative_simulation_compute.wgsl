/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

// input
@group(0) @binding(0) var<uniform> settings: FlowPySettings;
@group(0) @binding(1) var height_texture: texture_2d<f32>;
@group(0) @binding(2) var release_point_texture: texture_2d<f32>;
@group(0) @binding(3) var<storage, read> input_parents: array<u32>;

//output
@group(0) @binding(4) var<storage, read_write> flux: array<atomic<u32>>;
@group(0) @binding(5) var<storage, read_write> output_parents: array<atomic<u32>>;
@group(0) @binding(6) var output_texture: texture_storage_2d<rgba8unorm, write>; // ASSERT: same dimensions as height_texture

struct FlowPySettings {
    num_iteration: u32,
    padding1: u32,
    padding2: u32,
    padding3: u32,
}

fn pos_to_index(pos: vec2u) -> u32 {
    return pos.y * textureDimensions(height_texture).x + pos.x;
}

// test model for debugging
fn compute_flow_step_test(pos: vec2u) {
    if (any(pos < vec2u(1)) || any(pos > textureDimensions(height_texture) - vec2u(1))) {
        // we are near border, not all neighbors are defined
        return;
    }

    if (settings.num_iteration == 0) {
        let is_release_point = textureLoad(release_point_texture, pos, 0).a > 0;
        atomicStore(&flux[pos_to_index(pos)], u32(is_release_point));
        textureStore(output_texture, pos, vec4f(f32(is_release_point), 0, 0, f32(is_release_point)));
    } else {
        let base_cell_flux = atomicExchange(&flux[pos_to_index(pos)], 0u);        

        if (base_cell_flux == 0u) {
            return;
        }
        
        let is_release_point = textureLoad(release_point_texture, pos, 0).a > 0;
        textureStore(output_texture, pos, vec4f(f32(is_release_point), 0, 1, 1));

        // check neighbors
        let top_left = vec2u(vec2i(pos) + vec2i(-1, -1));
        let top_center = vec2u(vec2i(pos) + vec2i(0, -1));
        let top_right = vec2u(vec2i(pos) + vec2i(1, -1));
        let center_left = vec2u(vec2i(pos) + vec2i(-1, 0));
        let center_right = vec2u(vec2i(pos) + vec2i(1, 0));
        let bottom_left = vec2u(vec2i(pos) + vec2i(-1, 1));
        let bottom_center = vec2u(vec2i(pos) + vec2i(0, 1));
        let bottom_right = vec2u(vec2i(pos) + vec2i(1, 1));   

        let height_center = textureLoad(height_texture, pos, 0).r;

        let height_top_left = textureLoad(height_texture, top_left, 0).r;
        let height_top_center = textureLoad(height_texture, top_center, 0).r;
        let height_top_right = textureLoad(height_texture, top_right, 0).r;
        let height_center_left = textureLoad(height_texture, center_left, 0).r;
        let height_center_right = textureLoad(height_texture, center_right, 0).r;
        let height_bottom_left = textureLoad(height_texture, bottom_left, 0).r;
        let height_bottom_center = textureLoad(height_texture, bottom_center, 0).r;
        let height_bottom_right = textureLoad(height_texture, bottom_right, 0).r;

        if (height_center > height_top_left) {
            atomicAdd(&flux[pos_to_index(top_left)], base_cell_flux);
        }
        if (height_center > height_top_center) {
            atomicAdd(&flux[pos_to_index(top_center)], base_cell_flux);
        }
        if (height_center > height_top_right) {
            atomicAdd(&flux[pos_to_index(top_right)], base_cell_flux);
        }
        if (height_center > height_center_left) {
            atomicAdd(&flux[pos_to_index(center_left)], base_cell_flux);
        }
        if (height_center > height_center_right) {
            atomicAdd(&flux[pos_to_index(center_right)], base_cell_flux);
        }
        if (height_center > height_bottom_left) {
            atomicAdd(&flux[pos_to_index(bottom_left)], base_cell_flux);
        }
        if (height_center > height_bottom_center) {
            atomicAdd(&flux[pos_to_index(bottom_center)], base_cell_flux);
        }
        if (height_center > height_bottom_right) {
            atomicAdd(&flux[pos_to_index(bottom_right)], base_cell_flux);
        }
    }
}


fn compute_flow_step(pos: vec2u) {
    // Computes a single step of FlowPy
    // FlowPy usually computes the flow path from all starting cells separately. It then adds cells to the flow path serially.
    // We, on the other hand, on each step TODO

    //TODO basically do what avaframe.com4FlowPy.flowClass, method calc_distribution() does

    let parents = input_parents[pos_to_index(pos)];
    if (parents == 0) {
        return;
    }

    for (var i = 0u; i < 8u; i++) {
        let is_parent_set = parents & (1u << i);
        //if (is_parent_set) {
            // last iteration, some cell added itself as parent of this cell
        //}
    }

    //TODO calc z delta for each neighbor
    //TODO calc persistence
    //TODO calc tan beta
    //TODO (if not starting cell) calc fp travel angle
    //TODO (if not starting cell) calc sl travel angle
}

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    // id.xy in [0, ceil(texture_dimensions(normals_texture).xy / workgroup_size.xy) - 1]

    // exit if thread id is outside image dimensions (i.e. thread is not supposed to be doing any work)
    let texture_size = textureDimensions(height_texture);
    if (id.x >= texture_size.x || id.y >= texture_size.y) {
        return;
    }
    // id.xy in [0, texture_dimensions(output_tiles) - 1]
    
    compute_flow_step_test(id.xy);
}

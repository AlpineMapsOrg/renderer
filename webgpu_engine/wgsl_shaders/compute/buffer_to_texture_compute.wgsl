/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "util/color_mapping.wgsl"
#include "util/encoder.wgsl"

// input
@group(0) @binding(0) var<uniform> settings: BufferToTextureSettings;
@group(0) @binding(1) var<storage, read> input_storage_buffer: array<u32>;
@group(0) @binding(2) var<storage, read> input_transparency_buffer: array<u32>;

// output
@group(0) @binding(5) var output_texture: texture_storage_2d<rgba8unorm, write>;

struct BufferToTextureSettings {
    input_resolution: vec2u,
    color_map_bounds: vec2f,
    transparency_map_bounds: vec2f,
    use_bin_interpolation: u32,
    use_transparency_buffer: u32,
}

fn index_from_pos(pos: vec2u) -> u32 {
    return pos.y * settings.input_resolution.x + pos.x;
}

fn remap_clamped(x: f32, min_val: f32, max_val: f32) -> f32 {
    return clamp((x - min_val) / (max_val - min_val), 0.0, 1.0);
}

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    // id.xy in [0, ceil(input_resolution / workgroup_size.xy) - 1]

    // exit if thread id is outside image dimensions (i.e. thread is not supposed to be doing any work)
    if (id.x >= settings.input_resolution.x || id.y >= settings.input_resolution.y) {
        return;
    }
    // id.xy in [0, texture_dimensions(output_tiles) - 1]

    let buffer_index = index_from_pos(id.xy);

    const layer_scaling = 1.0; //1000.0; // scaling factor for layer values, to avoid precision loss (also change in avalanche_trajectories_compute)
    const calculate_velocity = true; // if true, calculate velocity based on grommis formula
    
    var alpha: f32 = 1.0;
    if (bool(settings.use_transparency_buffer)) {
        let transparency_value = f32(input_transparency_buffer[buffer_index]) / layer_scaling;
        alpha = remap_clamped(transparency_value, settings.transparency_map_bounds.x, settings.transparency_map_bounds.y);
    }

    var value = f32(input_storage_buffer[buffer_index]) / layer_scaling;
    if (calculate_velocity) {
        value = sqrt(value * 2.0 * 9.81); // calculate velocity based on grommis formula
    }
    var output_color = color_mapping_flowpy(value, settings.color_map_bounds.x, settings.color_map_bounds.y, bool(settings.use_bin_interpolation));
    output_color.a = output_color.a * alpha;

/*
    let risk_value = u32_to_range(read_buffer_at(id.xy), U32_ENCODING_RANGE_NORM);
    //let risk_value = f32(input_storage_buffer[buffer_index]) / 1000f;

    var output_color: vec4f;
    if (risk_value == 0.0) {
        output_color = vec4f(0.0);
    } else {
        output_color = vec4f(color_mapping_bergfex(risk_value), 1.0);
    }*/
    textureStore(output_texture, id.xy, output_color);
}
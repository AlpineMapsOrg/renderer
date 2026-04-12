/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Wendelin Muth
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

struct camera_config {
    view_matrix: mat4x4f,
    proj_matrix: mat4x4f,
    inv_view_matrix: mat4x4f,
    inv_proj_matrix: mat4x4f,
    position: vec4f,
}

struct accumulation_params {
    curr_camera: camera_config,
    prev_camera: camera_config,
    jitter: vec2f,
    prev_jitter: vec2f,
    low_res_texel_size: vec2f,
    high_res_texel_size: vec2f,
    resolution_scale: vec2f,
    _padding0: vec2f,
}

@group(0) @binding(0) var<uniform> params : accumulation_params;
@group(0) @binding(1) var current_color: texture_2d<f32>;
@group(0) @binding(2) var current_depth: texture_2d<f32>;
@group(0) @binding(3) var linear_sampler: sampler;
@group(0) @binding(4) var accumulation_color_r: texture_2d<f32>;
@group(0) @binding(5) var accumulation_color_w: texture_storage_2d<rgba16float, write>;

fn world_position_from_linear_depth(uv: vec2f, linear_depth: f32, inv_proj: mat4x4f, inv_view: mat4x4f) -> vec3f {
    // Convert UV to NDC
    let ndc_xy = uv * 2.0 - 1.0;

    let focal_x = 1.0 / inv_proj[0][0];
    let focal_y = 1.0 / inv_proj[1][1];

    let view_pos = vec4f(
        ndc_xy.x * linear_depth / focal_x,
        ndc_xy.y * linear_depth / focal_y,
        -linear_depth,
        1.0
    );

    // View to world space
    let world_pos = (inv_view * view_pos).xyz;
    return world_pos;
}

fn blend_transmittance(current_transmittance: f32, history_transmittance: f32, blend_factor: f32) -> f32 {
    let current_optical_depth = -log(max(current_transmittance, 0.00001));
    let history_optical_depth = -log(max(history_transmittance, 0.00001));
    let blended_optical_depth = mix(current_optical_depth, history_optical_depth, blend_factor);
    let blended_transmittance = exp(-blended_optical_depth);
    return blended_transmittance;
}

fn reproject_position(world_pos: vec3f, prev_view: mat4x4f, prev_proj: mat4x4f) -> vec3f {
    // World to previous view space
    let prev_view_pos = prev_view * vec4f(world_pos, 1.0);

    // View to clip space (unjittered projection)
    var prev_clip_pos = prev_proj * prev_view_pos;
    prev_clip_pos /= prev_clip_pos.w;

    // Convert to UV (unjittered)
    let prev_uv = prev_clip_pos.xy * 0.5 + 0.5;

    // Return unjittered UV and ndc z (unused here)
    return vec3f(prev_uv, prev_clip_pos.z);
}

fn clip_aabb(history_color: vec3f, aabb_min: vec3f, aabb_max: vec3f) -> vec3f {
    let p_clip = 0.5 * (aabb_max + aabb_min);
    let e_clip = 0.5 * (aabb_max - aabb_min) + 0.00001;

    let v_clip = history_color - p_clip;
    let v_unit = v_clip / e_clip;
    let a_unit = abs(v_unit);
    let ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0) {
        return p_clip + v_clip / ma_unit;
    }
    return history_color;
}

fn clip_scalar(history_value: f32, aabb_min: f32, aabb_max: f32) -> f32 {
    return clamp(history_value, aabb_min, aabb_max);
}

fn update_depth_stats(prev_data: vec2f, current_depth: f32, alpha: f32) -> vec2f {
    let next_mean = mix(prev_data.r, current_depth, alpha);
    let next_mean_sq = mix(prev_data.g, current_depth * current_depth, alpha);
    return vec2f(next_mean, next_mean_sq);
}

@compute @workgroup_size(8, 8, 1)
fn computeMain(@builtin(global_invocation_id) global_id: vec3u) {
    let high_res_dims = textureDimensions(accumulation_color_w);

    if (global_id.x >= high_res_dims.x || global_id.y >= high_res_dims.y) {
        return;
    }

    let high_res_coord = vec2i(global_id.xy);
    let high_res_uv = (vec2f(global_id.xy) + 0.5) / vec2f(high_res_dims);

    // Account for jitter in current sampling (assuming content shifted by -jitter, so sample at +jitter)
    let jittered_uv = high_res_uv - params.jitter * vec2f(1.0, -1.0);
    let jittered_coord = vec2i(jittered_uv * vec2f(textureDimensions(current_depth)));

    let current_sample = textureSampleLevel(current_color, linear_sampler, jittered_uv, 0.0);
    let current_depth = textureLoad(current_depth, jittered_coord, 0).r;

    // Early exit for background/sky
    if (current_depth <= 0.0) {
        textureStore(accumulation_color_w, high_res_coord, current_sample);
        return;
    }

    // Reconstruct world position from UNJITTERED position
    let world_pos = world_position_from_linear_depth(
        high_res_uv,  // Use unjittered UV for reconstruction
        current_depth,
        params.curr_camera.inv_proj_matrix,
        params.curr_camera.inv_view_matrix
    );

    // Reproject to previous frame (unjittered)
    let prev_position = reproject_position(
        world_pos,
        params.prev_camera.view_matrix,
        params.prev_camera.proj_matrix
    );

    // Apply previous frame's jitter to get actual texture coordinates for history
    let prev_uv = prev_position.xy;

    // Validate reprojection
    let is_valid = all(prev_uv >= vec2f(0.0)) && all(prev_uv <= vec2f(1.0));

    if (!is_valid) {
        textureStore(accumulation_color_w, high_res_coord, current_sample);
        return;
    }

    // Sample history (bilinear would require regular texture, but storage is point-sampled)
    let history_sample = textureSampleLevel(accumulation_color_r, linear_sampler, prev_uv, 0.0);

    // Variance clipping - sample 3x3 neighborhood for scattered and transmittance
    var scattered_sum = vec3f(0.0);
    var scattered_squared_sum = vec3f(0.0);
    var trans_sum = 0.0;
    var trans_squared_sum = 0.0;
    var weight_sum = 0.0;

    for (var dy = -1; dy <= 1; dy++) {
        for (var dx = -1; dx <= 1; dx++) {
            let offset = vec2f(f32(dx), f32(dy)) * params.low_res_texel_size;
            let neighbor_uv = jittered_uv + offset;
            let neighbor = textureSampleLevel(current_color, linear_sampler, neighbor_uv, 0.0);

            let opacity = max(1.0 - neighbor.a, 0.001);
            let neighbor_scattered = neighbor.rgb / opacity;

            // Gaussian weight (center = 1.0, edges = ~0.6, corners = ~0.36)
            let w = exp(-f32(dx * dx + dy * dy) * 0.5);

            scattered_sum += neighbor_scattered * w;
            scattered_squared_sum += neighbor_scattered * neighbor_scattered * w;
            trans_sum += neighbor.a * w;
            trans_squared_sum += neighbor.a * neighbor.a * w;
            weight_sum += w;
        }
    }

    let scattered_mean = scattered_sum / weight_sum;
    let scattered_variance = max((scattered_squared_sum / weight_sum) - (scattered_mean * scattered_mean), vec3f(0.0));
    let scattered_stddev = sqrt(scattered_variance);

    let trans_mean = trans_sum / weight_sum;
    let trans_variance = max((trans_squared_sum / weight_sum) - (trans_mean * trans_mean), 0.0);
    let trans_stddev = sqrt(trans_variance);

    // AABB for variance clipping (1.0 sigma)
    let gamma = 1.0;
    let scattered_aabb_min = scattered_mean - gamma * scattered_stddev;
    let scattered_aabb_max = scattered_mean + gamma * scattered_stddev;
    let trans_aabb_min = trans_mean - gamma * trans_stddev;
    let trans_aabb_max = trans_mean + gamma * trans_stddev;

    // Undo pre-multiply
    let current_opacity = max(1.0 - current_sample.a, 0.001);
    let history_opacity = max(1.0 - history_sample.a, 0.001);
    let current_scattered = current_sample.rgb / current_opacity;
    var history_scattered = history_sample.rgb / history_opacity;
    var history_transmittance = history_sample.a;

    // Calculate clip penalty using unclipped history
    let scattered_diff = length(history_scattered - current_scattered);
    let scattered_stddev_length = length(scattered_stddev) + 0.001;
    let scattered_clip_factor = 1.0 - smoothstep(scattered_stddev_length * 2.5, scattered_stddev_length * 3.5, scattered_diff);

    let trans_diff = abs(history_transmittance - current_sample.a);
    let trans_stddev_length = trans_stddev + 0.001;
    let trans_clip_factor = 1.0 - smoothstep(trans_stddev_length * 2.5, trans_stddev_length * 3.5, trans_diff);

    // Clip the history scattered and transmittance
    history_scattered = clip_aabb(history_scattered, scattered_aabb_min, scattered_aabb_max);
    history_transmittance = clip_scalar(history_transmittance, trans_aabb_min, trans_aabb_max);

    var history_weight = 0.95;
    history_weight *= max(min(trans_clip_factor, scattered_clip_factor), 0.5);

    // Special case: completely empty history
    if (history_sample.a > 0.999 && current_sample.a < 0.95) {
        history_weight = 0.0;
    }

    // Blend scattered
    let blended_scattered = mix(current_scattered, history_scattered, history_weight);

    // Blend transmittance
    let blended_transmittance = blend_transmittance(
        current_sample.a,
        history_transmittance,
        history_weight
    );

    let blended_color = blended_scattered * (1.0 - blended_transmittance);

    // Write output
    textureStore(accumulation_color_w, high_res_coord, vec4f(blended_color, blended_transmittance));
}
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

#include "util/tile_util.wgsl"
#include "util/shared_config.wgsl"
#include "util/atmosphere.wgsl"

struct tile_info {
    index: u32,
    zoom: u32,
}

struct camera_config {
    view_matrix: mat4x4f,
    proj_matrix: mat4x4f,
    inv_view_matrix: mat4x4f,
    inv_proj_matrix: mat4x4f,
    position: vec4f,
}

struct shader_params {
    camera: camera_config,
    bounds_min: vec4f,
    bounds_max: vec4f,

    frame_index: u32,
    scattering_coeff: f32,
    extinction_coeff: f32,
    albedo: f32,

    step_size_min: f32,
    step_size_distance_factor: f32,
    step_size_horizon_factor: f32,
    fade_factor: f32,

    sun_light_scale: f32,
    ambient_light_scale: f32,
    atm_light_scale: f32,
    shadow_extinction_scale: f32,

    jitter: vec2f,
    powder_scale: f32,
    padding: f32,
}

struct ray_accumulator {
    t: f32,
    radiance: vec3f,
    transmittance: f32,
    depth: f32,
    depth_weight: f32,

    // State machine flags
    searching_for_cloud: bool,
    consecutive_empty_steps: i32,
    mandatory_fine_dist: f32,
    step_count: i32,
}

@group(0) @binding(0) var<uniform> params : shader_params;
@group(0) @binding(1) var atlas_texture: texture_3d<f32>;
@group(0) @binding(2) var atlas_sampler_l: sampler;
@group(0) @binding(3) var<storage, read> tile_infos: array<tile_info>;
@group(0) @binding(4) var output_color: texture_storage_2d<rgba16float, write>;
@group(0) @binding(5) var output_depth: texture_storage_2d<r32float, write>;

@group(1) @binding(0) var depth_texture: texture_2d<f32>;

@group(2) @binding(0) var<uniform> sconf : shared_config;

// tile size at zoom level 10
override tile_size_xy = 39135.7584820102;
override inv_tile_size_xy = 1.0 / 39135.7584820102;

override tile_count_x = 46/2;
override tile_count_y = 26/2;

override zoom_max = 10;

override tile_coords_offset_x = 538;
override tile_coords_offset_y = 660;

override atlas_bits_xy = 2u;
override atlas_bits_z = 5u;

#define ATLAS_MASK_XY u32((1<<atlas_bits_xy)-1)
#define ATLAS_MASK_Z u32((1<<atlas_bits_z)-1)
#define ATLAS_INV_SCALE vec3(1.0 / f32(1<<atlas_bits_xy), 1.0 / f32(1<<atlas_bits_xy), 1.0 / f32(1<<atlas_bits_z))

// Texture dimensions
const TILE_RESOLUTION_XY = 256.0;
const INV_TILE_RESOLUTION_XY = 1.0 / TILE_RESOLUTION_XY;
const TILE_RESOLUTION_Z = 64.0;
const INV_TILE_RESOLUTION_Z = 1.0 / TILE_RESOLUTION_Z;
// Note: Using a non-cos-corrected constant here is correct
const HEIGHT_PER_TEXEL = 14000.0 / TILE_RESOLUTION_Z;
const INV_HEIGHT_PER_TEXEL = 1.0 / HEIGHT_PER_TEXEL;
const TEXTURE_VALUE_SCALE = 32.0; // Has to match generation script

// Ray marching parameters
const MAX_STEPS = 128;

const MAX_LIGHT_STEPS = 8;

fn unproject(normalised_device_coordinates: vec3f) -> vec3f {
    let unprojected = params.camera.inv_proj_matrix * vec4(normalised_device_coordinates, 1.0);
    let normalised_unprojected = unprojected / unprojected.w;
    return (params.camera.inv_view_matrix * normalised_unprojected).xyz;
}

// Box-ray intersection
fn intersect_aabb(ray_origin: vec3f, ray_dir: vec3f, box_min: vec3f, box_max: vec3f) -> vec2f {
    let inv_dir = 1.0 / ray_dir;
    let t0 = (box_min - ray_origin) * inv_dir;
    let t1 = (box_max - ray_origin) * inv_dir;
    let tmin = min(t0, t1);
    let tmax = max(t0, t1);
    let t_near = max(max(tmin.x, tmin.y), tmin.z);
    let t_far = min(min(tmax.x, tmax.y), tmax.z);
    return vec2f(t_near, t_far);
}

fn get_tile_id_at_pos(pos_world: vec3f) -> vec2i {
    // Equivalent to subtracting bounds_min, dividing and then flooring,
    // but avoids floating point mismatch to tile_uv calculation.
    let pos_ts = pos_world.xy * inv_tile_size_xy;
    return vec2i(floor(pos_ts)) - vec2i(tile_coords_offset_x, tile_coords_offset_y);
}

fn get_tile_info(tile_id: vec2i) -> tile_info {
    if(tile_id.x < 0 || tile_id.x >= tile_count_x || tile_id.y < 0 || tile_id.y >= tile_count_y) {
        var default_tile: tile_info;
        default_tile.index = 0u;
        default_tile.zoom = u32(0u);
        return default_tile;
    }

    let tile_index = tile_id.x + tile_id.y * tile_count_x;
    return tile_infos[tile_index];
}

fn sample_volume(pos_world: vec3f, lod: f32, tile_id: vec2i, tile: tile_info, atlas_sampler: sampler) -> f32 {
    let height_adjusted = pos_world.z * cos(y_to_lat(pos_world.y));
    if (height_adjusted < 0.0 || height_adjusted > 14000.0 || tile.zoom == 0u) {
        return 0.0;
    }

    let dz = max(u32(zoom_max) - tile.zoom, 0u);
    let tile_scale = f32(1u << dz);

    let pos_ts = pos_world.xy * inv_tile_size_xy / tile_scale;
    let tile_uv = fract(pos_ts);

    let atlas_pos = vec3u(
        tile.index & ATLAS_MASK_XY,
        (tile.index >> atlas_bits_xy) & ATLAS_MASK_XY,
        (tile.index >> (atlas_bits_xy << 1u)) & ATLAS_MASK_Z
    );

    // This projects into texture space height which is 0-14000.
    let height_normalized = height_adjusted * INV_HEIGHT_PER_TEXEL * INV_TILE_RESOLUTION_Z;

    let mip_scale = exp2(lod);
    let texel_size = mip_scale * vec3f(INV_TILE_RESOLUTION_XY, INV_TILE_RESOLUTION_XY, INV_TILE_RESOLUTION_Z);
    let safe_uvw = clamp(vec3f(tile_uv, height_normalized), texel_size, vec3f(1.0) - texel_size);
    let atlas_uvw = (safe_uvw + vec3f(atlas_pos)) * ATLAS_INV_SCALE;

    let raw = textureSampleLevel(atlas_texture, atlas_sampler, atlas_uvw, lod).r;
    return raw * TEXTURE_VALUE_SCALE * 0.001;
}

fn calculate_lod(step_size: f32, tile_zoom: u32, distance: f32, view_dir: vec3f) -> f32 {
    let dz = max(u32(zoom_max) - tile_zoom, 0u);
    let tile_size_multiplier = f32(1u << dz);

    let texel_size_xy = (tile_size_xy * tile_size_multiplier) / TILE_RESOLUTION_XY;
    let texel_size_z = HEIGHT_PER_TEXEL;

    let view_xy = length(view_dir.xy);
    let view_z = abs(view_dir.z);

    // Texels traversed per step along each axis
    // Small value = undersampling that axis = need fine LOD
    let texels_per_step_xy = (step_size * view_xy) / texel_size_xy;
    let texels_per_step_z  = (step_size * view_z)  / texel_size_z;

    let lod_xy = log2(max(texels_per_step_xy / 2.0, 1.0));
    let lod_z  = log2(max(texels_per_step_z  / 2.0, 1.0));

    // Use the finest LOD required by either axis
    return clamp(min(lod_xy, lod_z) - 0.5, 0.0, 5.0);
}

// Saturate helper
fn saturate(x: f32) -> f32 {
    return clamp(x, 0.0, 1.0);
}

// Henyey-Greenstein phase function for anisotropic scattering
fn henyey_greenstein_phase(cos_angle: f32, g: f32) -> f32 {
    let g2 = g * g;
    let denom = 1.0 + g2 - 2.0 * g * cos_angle;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(denom, 1.5));
}

// Improved phase function with better forward and back scattering
fn cloud_phase_function(cos_angle: f32) -> f32 {
    let forward = henyey_greenstein_phase(cos_angle, params.scattering_coeff);
    let backward = henyey_greenstein_phase(cos_angle, -params.scattering_coeff * 0.5);
    let isotropic = 0.25 / 3.14159265;
    return forward * 0.5 + backward * 0.2 + isotropic * 0.3;
}

// Calculate how much light reaches a point from the sun (light transmittance)
// Uses cone-based sampling with decreasing LOD as per Nubis/Guerrilla Games approach
fn sample_light_energy(pos: vec3f, sun_dir: vec3f, extinction_coeff: f32, base_lod: f32, start_t: f32, cos_angle: f32) -> f32 {
    if (sun_dir.z <= 0.0) {
        return 0.0;
    }

    if (pos.z >= params.bounds_max.z) {
        return 1.0;
    }

    // Calculate maximum ray length to volume boundary
    let max_ray_length = min((params.bounds_max.z - pos.z) / sun_dir.z, 10000.0);
    // Initial step size derived from geometric series sum to exactly span max_ray_length
    const GROWTH_FACTOR = 1.5;
    const STEP_SIZE_CONSTANT = (GROWTH_FACTOR - 1.0) / (pow(GROWTH_FACTOR, f32(MAX_LIGHT_STEPS)) - 1.0);
    var step_size = max_ray_length * STEP_SIZE_CONSTANT;

    var optical_depth = 0.0;
    var t = (start_t + step_size) * 0.5;

    // March towards sun with decreasing LOD per Nubis approach
    // The decreasing LOD smooths out artifacts from sparse sampling
    for (var i = 0; i < MAX_LIGHT_STEPS; i++) {
        let sample_pos = pos + sun_dir * t;

        // Get tile info for this sample
        let tile_id = get_tile_id_at_pos(sample_pos);
        let tile = get_tile_info(tile_id);

        // Calculate base LOD, then add increasing bias per step
        // This decreasing detail level smooths transitions between samples
        let lod_bias = max(f32(i) * 0.5 - 0.5, 0.0);  // Increase LOD by 0.5 per step
        let lod = base_lod + lod_bias;

        // Sample density with calculated LOD
        let density = sample_volume(sample_pos, lod, tile_id, tile, atlas_sampler_l);

        // Accumulate optical depth
        optical_depth += density * step_size * extinction_coeff * params.shadow_extinction_scale;

        // Early exit for deep shadows
        if (optical_depth > 20.0) {
            return 0.0;
        }

        t += step_size;
        step_size *= GROWTH_FACTOR;
    }

    let beer = exp(-optical_depth);
    let powder = 1.0 - exp(-optical_depth * 2.0);
    let powder_weight = smoothstep(0.5, -0.5, cos_angle) * params.powder_scale;
    let primary = mix(beer, beer * powder * 2.0, powder_weight);

    // Secondary/MS: softer attenuation curve, but directional,
    // suppress when looking toward sun, matching Horizon's approach
    let secondary = exp(-optical_depth * 0.25) * 0.7;
    let secondary_directional = secondary * smoothstep(-0.5, 0.7, cos_angle);

    // secondary can only brighten relative to primary, never push total above 1
    return max(primary, secondary_directional);
}

// Dynamic step size based on distance
fn get_step_size(distance: f32, ray_direction: vec3f, ray_length: f32) -> f32 {
    let horizon = 1.0 - abs(ray_direction.z); // 1.0 at horizontal, 0.0 at vertical
    let horizon_bonus = horizon * horizon * params.step_size_horizon_factor;
    let distance_based = max(distance * params.step_size_distance_factor, params.step_size_min);
    let length_based = ray_length / 32.0;
    return min(distance_based + horizon_bonus, length_based);
}

// Convert world position to NDC depth for storage
fn world_to_depth(world_pos: vec3f) -> f32 {
    let view_pos = params.camera.view_matrix * vec4f(world_pos, 1.0);
    let clip_pos = params.camera.proj_matrix * view_pos;
    let ndc = clip_pos.xyz / clip_pos.w;
    return ndc.z;
}

fn ign(pixel: vec2f) -> f32 {
    return fract(52.9829189f * fract(dot(pixel, vec2f(0.06711056f, 0.00583715f))));
}

fn r1_sequence(n: u32) -> f32 {
    let g = 1.6180339887498948482;
    let a1 = 1.0 / g;
    return fract(0.5+a1*f32(n));
}

// Combined spatial + temporal noise
fn get_ray_offset(pixel: vec2u, frame: u32) -> f32 {
    let temporal = r1_sequence(frame);
    let spatial = ign(vec2f(pixel));
    return fract(spatial + temporal);
}

fn calculate_point_radiance(
    pos: vec3f,
    beta: f32,
    sun_dir: vec3f,
    lod: f32,
    step_size: f32,
    jitter: f32,
    cloud_phase: f32,
    cos_angle: f32
) -> vec3f {
    let cloud_extinction = beta * params.extinction_coeff;
    let cloud_scattering = cloud_extinction * params.albedo;

    let cloud_sun_transmittance = sample_light_energy(pos, sun_dir, params.extinction_coeff, lod, step_size, cos_angle);

    let sun_radiance = sconf.sun_light.rgb * sconf.sun_light.a * params.sun_light_scale;
    let cloud_sun_inscatter = sun_radiance * cloud_sun_transmittance * cloud_phase;

    // --- Ambient ---
    // Ambient: only modulate by height, since we have no way to estimate
    // depth-into-cloud from density alone. Cloud tops receive more sky light.
    let height_factor = saturate(pos.z / params.bounds_max.z);
    let ambient_occlusion = mix(0.3, 1.0, height_factor);

    let ambient_radiance = sconf.amb_light.rgb * sconf.amb_light.a * params.ambient_light_scale;
    var ambient_color = ambient_radiance;
    if (bool(sconf.atmosphere_enabled)) {
        let pos_km = pos / 1000.0;
        let air_density = density_at_height(pos_km.z);
        let rayleigh_coeff = scattering_coefficients();
        let atm_scattering = air_density * rayleigh_coeff;
        let atm_inscatter_density = atmospheric_inscatter_at_point(pos_km, sun_dir);
        let atm_tint = sun_radiance * atm_inscatter_density * atm_scattering * params.atm_light_scale;
        // Lerp toward tint rather than adding — controls saturation
        ambient_color = mix(ambient_radiance, atm_tint, 0.2);
    }

    let cloud_ambient_inscatter = ambient_color * ambient_occlusion;

    let cloud_total_inscatter = cloud_sun_inscatter + cloud_ambient_inscatter;
    return cloud_total_inscatter * cloud_scattering * step_size;
}

fn step_coarse(
    ray_origin: vec3f,
    ray_dir: vec3f,
    fine_step_size: f32,
    t_far: f32,
    ray_jitter: f32,
    acc: ptr<function, ray_accumulator>
) -> bool {
    let big_step = fine_step_size * 4.0;

    // Jitter within the big step
    let sample_t = (*acc).t + big_step * ray_jitter;

    // Boundary check
    if (sample_t >= t_far) {
        (*acc).t += big_step;
        return false; // Did not find cloud, but safe to continue
    }

    let pos = ray_origin + ray_dir * sample_t;
    let tile_id = get_tile_id_at_pos(pos);
    let tile = get_tile_info(tile_id);

    // Calculate Coarse LOD
    let base_lod = calculate_lod(fine_step_size, tile.zoom, sample_t, ray_dir);
    let coarse_lod = min(base_lod + 3.0, 5.0);

    let coarse_density = sample_volume(pos, coarse_lod, tile_id, tile, atlas_sampler_l);

    if (coarse_density > 0.0) {
        // HIT: Switch state to Fine
        (*acc).searching_for_cloud = false;
        (*acc).mandatory_fine_dist = big_step;
        (*acc).consecutive_empty_steps = 0;
        // Do NOT advance t. We return true to tell the caller "Loop again immediately"
        return true;
    }

    (*acc).t += big_step;
    return false;
}

fn step_fine(
    ray_origin: vec3f,
    ray_dir: vec3f,
    sun_dir: vec3f,
    fine_step_size: f32,
    t_far: f32,
    fade_params: vec2f, // x: near, y: far
    cloud_phase: f32,
    cos_angle: f32,
    ray_jitter: f32,
    acc: ptr<function, ray_accumulator>
)  {
      let sample_t = min((*acc).t + fine_step_size * ray_jitter, t_far);

      let pos = ray_origin + ray_dir * sample_t;
      let tile_id = get_tile_id_at_pos(pos);
      let tile = get_tile_info(tile_id);

      let lod = calculate_lod(fine_step_size, tile.zoom, sample_t, ray_dir);

      // Volume camera fade
      let dist_cylinder = max(length(pos.xy - ray_origin.xy), (ray_origin.z - pos.z) * 0.5);
      let fade_t = saturate((dist_cylinder - fade_params.x) / (fade_params.y - fade_params.x));
      let fade = fade_t * fade_t * fade_t;

      let base_beta = sample_volume(pos, lod, tile_id, tile, atlas_sampler_l);
      let beta = base_beta * fade;

      if (beta > 0.0) {
          (*acc).consecutive_empty_steps = 0;

          let radiance_contribution = calculate_point_radiance(
              pos, beta, sun_dir, lod, fine_step_size, ray_jitter, cloud_phase, cos_angle
          );

          // Accumulate Light
          (*acc).radiance += radiance_contribution * (*acc).transmittance;

          // Accumulate Depth
          (*acc).depth += sample_t * (*acc).transmittance;
          (*acc).depth_weight += (*acc).transmittance;

          // Apply Extinction
          let extinction = beta * params.extinction_coeff * fine_step_size;
          (*acc).transmittance *= exp(-extinction);
      } else {
          (*acc).consecutive_empty_steps++;
      }

      // Update Space Skipping State
      (*acc).mandatory_fine_dist -= fine_step_size;

      // Switch back to coarse if we are in empty space and have cleared the mandatory zone
      if ((*acc).mandatory_fine_dist <= 0.0 && (*acc).consecutive_empty_steps >= 8) {
          (*acc).searching_for_cloud = true;
      }

      (*acc).t += fine_step_size;
  }

@compute @workgroup_size(8, 8, 1)
fn computeMain(@builtin(global_invocation_id) global_id: vec3u) {
    let output_dims = textureDimensions(output_color);
    let pixel_coord = global_id.xy;

    if (pixel_coord.x >= output_dims.x || pixel_coord.y >= output_dims.y) {
        return;
    }

    let pixel_center = vec2f(pixel_coord) + 0.5;
    let texcoords = pixel_center / vec2f(output_dims);

    let ray_jitter = get_ray_offset(pixel_coord, params.frame_index);

    let origin = params.camera.position.xyz;
    let stable_depth_coord = 2 * vec2i(pixel_coord) - vec2i(2.0 * params.jitter);

    let frag_depth = max(textureLoad(depth_texture, stable_depth_coord, 0).x, 1e-6f);
    let frag_pos = unproject(vec3f(texcoords * vec2f(2.0, -2.0) + vec2f(-1.0, 1.0), frag_depth));
    let ray_direction = normalize(frag_pos);

    let fade_near = 1000.0 * params.fade_factor;
    let fade_far = 50000.0 * params.fade_factor;

    // Intersect ray with volume
    let intersection = intersect_aabb(origin, ray_direction, params.bounds_min.xyz, params.bounds_max.xyz);
    let t_near = max(intersection.x, fade_near);
    let t_far = min(intersection.y, length(frag_pos));

    if (t_near >= t_far) {
        textureStore(output_color, pixel_coord, vec4f(0.0, 0.0, 0.0, 1.0));
        textureStore(output_depth, pixel_coord, vec4f(0.0, 0.0, 0.0, 0.0));
        return;
    }

    var acc: ray_accumulator;
    acc.t = t_near;
    acc.radiance = vec3f(0.0);
    acc.transmittance = 1.0;
    acc.depth = 0.0;
    acc.depth_weight = 0.0;
    acc.searching_for_cloud = true;
    acc.consecutive_empty_steps = 0;
    acc.mandatory_fine_dist = 0.0;
    acc.step_count = 0;

    let sun_dir = normalize(-sconf.sun_light_dir.xyz);

    // Phase function (view-dependent scattering)
    let cos_angle = dot(ray_direction, sun_dir);
    let cloud_phase = cloud_phase_function(cos_angle);

    while (acc.transmittance > 0.01 && acc.t < t_far && acc.step_count < MAX_STEPS) {
        // Calculate Step Size
        var fine_step_size = get_step_size(acc.t, ray_direction, t_far - t_near);
        fine_step_size = min(fine_step_size, t_far - acc.t);

        if (fine_step_size < 0.01) { break; }

        if (acc.searching_for_cloud) {
            // Returns true if hit, meaning we should loop again without advancing
            let hit = step_coarse(origin, ray_direction, fine_step_size, t_far, ray_jitter, &acc);
            if (hit) { continue; }
        } else {
             step_fine(
                origin, ray_direction, sun_dir,
                fine_step_size, t_far, vec2f(fade_near, fade_far),
                cloud_phase, cos_angle, ray_jitter, &acc
            );
        }

        acc.step_count++;
    }

    // Note: accumulated radiance is already "alpha-premultiplied" in a sense
    textureStore(output_color, pixel_coord, vec4f(acc.radiance, acc.transmittance));

    // Output apparent depth (linear depth in NDC Z)
    var apparent_depth = min(acc.depth / acc.depth_weight, t_far);
    if(acc.depth_weight == 0.0) {
        apparent_depth = t_far;
    }
    textureStore(output_depth, pixel_coord, vec4f(apparent_depth, 0.0, 0.0, 0.0));
}

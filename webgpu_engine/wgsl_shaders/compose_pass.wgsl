/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2022 Gerald Kimmersdorfer
 * Copyright (C) 2024 Patrick Komon
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

#include "util/shared_config.wgsl"
#include "util/camera_config.wgsl"
#include "util/atmosphere.wgsl"
#include "util/encoder.wgsl"
#include "util/snow.wgsl"
#include "util/tile_util.wgsl"

#include "screen_pass_vert.wgsl"

@group(0) @binding(0) var<uniform> conf : shared_config;
@group(1) @binding(0) var<uniform> camera : camera_config;

@group(2) @binding(0) var albedo_texture : texture_2d<u32>;
@group(2) @binding(1) var position_texture : texture_2d<f32>;
@group(2) @binding(2) var normal_texture : texture_2d<u32>;
@group(2) @binding(3) var atmosphere_texture : texture_2d<f32>;
@group(2) @binding(4) var overlay_texture : texture_2d<u32>;

@group(2) @binding(5) var<uniform> image_overlay_settings: ImageOverlaySettings;
@group(2) @binding(6) var image_overlay_texture: texture_2d<f32>;
@group(2) @binding(7) var image_overlay_sampler: sampler;

@group(2) @binding(8) var<uniform> compute_overlay_settings: ImageOverlaySettings;
@group(2) @binding(9) var compute_overlay_texture: texture_2d<f32>;
@group(2) @binding(10) var compute_overlay_sampler: sampler;

@group(2) @binding(11) var clouds_texture: texture_2d<f32>;
@group(2) @binding(12) var clouds_depth_texture: texture_storage_2d<r32float, read>;
@group(2) @binding(13) var cloud_shadow_texture: texture_2d<f32>;
@group(2) @binding(14) var cloud_shadow_sampler: sampler;
@group(2) @binding(15) var depth_texture: texture_2d<f32>;

const CLOUD_SHADOW_AABB_MIN = vec3f(1045658.54694121, 5811660.13457852, 0.0);
const CLOUD_SHADOW_AABB_MAX = vec3f(1937220.04485951, 6309418.06277159, 14000.0);

struct ImageOverlaySettings {
    aabb_min: vec2f,
    aabb_max: vec2f,
    alpha: f32,
    mode: u32, // 0: overlay, 1: encoded float
    float_decoding_lower_bound: f32,
    float_decoding_upper_bound: f32,
    texture_size: vec2<f32>,
    padding: vec2<f32>
}

// Calculates the diffuse and specular illumination contribution for the given
// parameters according to the Blinn-Phong lighting model.
// All parameters must be normalized.
fn calc_blinn_phong_contribution(
    toLight: vec3<f32>, 
    toEye: vec3<f32>, 
    normal: vec3<f32>, 
    diffFactor: vec3<f32>, 
    specFactor: vec3<f32>, 
    specShininess: f32
) -> vec3<f32> {
    let nDotL: f32 = max(0.0, dot(normal, toLight)); // Lambertian coefficient
    let h: vec3<f32> = normalize(toLight + toEye);
    let nDotH: f32 = max(0.0, dot(normal, h));
    let specPower: f32 = pow(nDotH, specShininess);
    let diffuse: vec3<f32> = diffFactor * nDotL; // Component-wise product
    let specular: vec3<f32> = specFactor * specPower;
    return diffuse + specular;
}

// Calculates the Blinn-Phong illumination for the given fragment
fn calculate_illumination(
    albedo: vec3<f32>, 
    eyePos: vec3<f32>, 
    fragPos: vec3<f32>, 
    fragNorm: vec3<f32>, 
    dirLight: vec4<f32>, 
    ambLight: vec4<f32>, 
    dirDirection: vec3<f32>, 
    material: vec4<f32>, 
    ao: f32, 
    shadow_term: f32
) -> vec3<f32> {
    let dirColor: vec3<f32> = dirLight.rgb * dirLight.a;
    let ambColor: vec3<f32> = ambLight.rgb * ambLight.a;
    let ambient: vec3<f32> = material.r * albedo;
    let diff: vec3<f32> = material.g * albedo;
    let spec: vec3<f32> = vec3<f32>(material.b);
    let shini: f32 = material.a;

    let ambientIllumination: vec3<f32> = ambient * ambColor * ao;

    let toLightDirWS: vec3<f32> = -normalize(dirDirection);
    let toEyeNrmWS: vec3<f32> = normalize(eyePos - fragPos);
    let diffAndSpecIllumination: vec3<f32> = dirColor * calc_blinn_phong_contribution(toLightDirWS, toEyeNrmWS, fragNorm, diff, spec, shini);

    return ambientIllumination + diffAndSpecIllumination * (1.0 - shadow_term);
}

fn apply_height_lines(out_Color: ptr<function, vec4f>, pos_ws: vec3f, normal: vec3f, dist: f32, interval: f32, base_width: f32, darkening_factor: f32, draw_line: ptr<function, bool>, aa_scale: f32) {
    if (!(*draw_line)) {
        return;
    }
    let alpha_line = 1.0 - min(dist / 20000.0, 1.0);
    var line_width = (1.0 + dist / 5000.0) * base_width;
    var aa_scaled = 0.0;
    if (aa_scale > 0.0) {
        aa_scaled = max(dist / 2000.0 * aa_scale, 0.03);
    }
    
    // Adjust line width by steepness (angle from up)
    let steepness = acos(clamp(normal.z, -1.0, 1.0)); 
    line_width = line_width * max(0.01, steepness);

    if (alpha_line > 0.01) {
        let latitude_correction = cos(y_to_lat(pos_ws.y));
        let altitude = pos_ws.z * latitude_correction;
        let fractional_part = altitude - f32(i32(altitude / interval)) * interval;
        //distance to line center
        let dist_from_line = min(fractional_part, interval - fractional_part);
        //distance to line edge: 0 = edge, positive = inside line
        let dist_from_edge = (line_width / 2.0) - dist_from_line;
        if (abs(dist_from_edge) < aa_scaled) {
            let aa_factor = smoothstep(-aa_scaled, aa_scaled, dist_from_edge);
            let effective_alpha = alpha_line * aa_factor;
            if (effective_alpha > 0.01) {
                let darken = (*out_Color).rgb - vec3f(darkening_factor);
                *out_Color = vec4f(mix((*out_Color).rgb, darken, effective_alpha), (*out_Color).a);
                *draw_line = false;
            }
        } 
        else if (dist_from_edge > 0.0) {
            let darken = (*out_Color).rgb - vec3f(darkening_factor);
            *out_Color = vec4f(mix((*out_Color).rgb, darken, alpha_line), (*out_Color).a);
            *draw_line = false;
        }
        
    }
}

fn decode_rgba_to_normalized_value(rgba: vec4<f32>) -> f32 {
    let rgba_u8: vec4<u32> = vec4<u32>(rgba * 255.0);
    let packed_value: u32 = (rgba_u8.r << 24) | (rgba_u8.g << 16) | (rgba_u8.b << 8) | rgba_u8.a;
    return u32_to_range(packed_value, U32_ENCODING_RANGE_VALIDATION);
}

fn get_cloud_shadow_occlusion(world_pos: vec3f) -> f32 {
    const SHADOW_BIAS = 0.05;
    const ESM_CONSTANT = 4.0;

    // TODO: Future improvement: Implement parallax

    let uv = vec2f(
        (world_pos.x - CLOUD_SHADOW_AABB_MIN.x) / (CLOUD_SHADOW_AABB_MAX.x - CLOUD_SHADOW_AABB_MIN.x),
        (CLOUD_SHADOW_AABB_MAX.y - world_pos.y) / (CLOUD_SHADOW_AABB_MAX.y - CLOUD_SHADOW_AABB_MIN.y)
    );

    let shadow_map_val = textureSample(cloud_shadow_texture, cloud_shadow_sampler, uv).r;

    let height_adjusted = world_pos.z / cos(y_to_lat(world_pos.y));
    let h_receiver_norm = height_adjusted / CLOUD_SHADOW_AABB_MAX.z + SHADOW_BIAS;
    let receiver_val = exp(ESM_CONSTANT * h_receiver_norm);

    // factor from 0.0 (in shadow) to 1.0 (lit)
    let shadow_factor = clamp(receiver_val / shadow_map_val, 0.0, 1.0);

    if (world_pos.x < CLOUD_SHADOW_AABB_MIN.x || world_pos.x > CLOUD_SHADOW_AABB_MAX.x ||
        world_pos.y < CLOUD_SHADOW_AABB_MIN.y || world_pos.y > CLOUD_SHADOW_AABB_MAX.y) {
        return 0.0;
    }

    return 1.0 - shadow_factor;
}

@fragment
fn fragmentMain(vertex_out : VertexOut) -> @location(0) vec4f {
    let tci : vec2<u32> = vec2u(vertex_out.texcoords * camera.viewport_size);

    var albedo: vec3f = unpack4x8unorm(textureLoad(albedo_texture, tci, 0).r).xyz;
    let pos_dist = textureLoad(position_texture, tci, 0);
    let encoded_normal = textureLoad(normal_texture, tci, 0).xy;

    let pos_cws = pos_dist.xyz;
    let dist = length(pos_cws); // pos_dist.w
    let tile_dist = pos_dist.w;

    let normal = octNormalDecode2u16(encoded_normal);
    
    var amb_occlusion = 1.0;
    /* TODO: Implement ambient occlusion
    if (bool(conf.ssao_enabled)) {
        amb_occlusion = texture(texin_ssao, texcoords).r;
    }*/
    
    let sampled_shadow_layer: i32 = -1;
    
    let origin = camera.position.xyz;
    let pos_ws = pos_cws + origin;

    var out_Color = vec4f(0.0);
    let atmospheric_color = textureLoad(atmosphere_texture, vec2u(0,tci.y), 0).rgb;

    // sampling from texture needs to happen in uniform control flow, therefore this is outside the if
    // later, the sampled value is only used if we are in the overlay region (specified in image overlay settings uniform)
    var image_overlay_color = vec4f(0.0);
    {
        // TODO: calculate size in double on CPU. Maybe gonna fix alignment issue
        let image_overlay_uv = (pos_ws.xy - image_overlay_settings.aabb_min) / (image_overlay_settings.aabb_max - image_overlay_settings.aabb_min);
        
        let image_overlay_uv_px = image_overlay_uv * image_overlay_settings.texture_size;
        let dx: vec2<f32> = dpdx(image_overlay_uv_px);
        let dy: vec2<f32> = dpdy(image_overlay_uv_px);
        var mip_level: f32 = max(0.0, log2(max(length(dx), length(dy))));
        image_overlay_color = textureSampleLevel(image_overlay_texture, image_overlay_sampler, vec2f(image_overlay_uv.x, 1 - image_overlay_uv.y), mip_level);
    }

    var compute_overlay_color = vec4f(0.0);
    {
        // TODO: calculate size in double on CPU. Maybe gonna fix alignment issue
        let compute_overlay_uv = (pos_ws.xy - compute_overlay_settings.aabb_min) / (compute_overlay_settings.aabb_max - compute_overlay_settings.aabb_min);
        
        let compute_overlay_uv_px = compute_overlay_uv * compute_overlay_settings.texture_size;
        let dx: vec2<f32> = dpdx(compute_overlay_uv_px);
        let dy: vec2<f32> = dpdy(compute_overlay_uv_px);
        var mip_level: f32 = max(0.0, log2(max(length(dx), length(dy))));
        compute_overlay_color = textureSampleLevel(compute_overlay_texture, compute_overlay_sampler, vec2f(compute_overlay_uv.x, 1.0 - compute_overlay_uv.y), mip_level);
    }

    var cloud_shadow = 0.0;
    if (bool(conf.clouds_enabled)) {
        // must be called from uniform control flow :(
        let cloud_shadow_raw = get_cloud_shadow_occlusion(pos_ws);

        // Make it softer
        cloud_shadow = cloud_shadow_raw * cloud_shadow_raw * cloud_shadow_raw * cloud_shadow_raw;
    }

    // Don't do shading if not visible anyway and also don't for pixels where there is no geometry (depth==0.0)
    if (dist > 0.0) {
        let ray_direction = pos_cws / dist;
        var material_light_response = conf.material_light_response;

        // Apply material color by blending with albedo
        albedo = mix(albedo, conf.material_color.rgb, conf.material_color.a);

        var shadow_term = cloud_shadow;
        amb_occlusion *= 1.0 - cloud_shadow * 0.3;

        /*TODO: implement shadow
        if (bool(conf.csm_enabled)) {
            shadow_term = csm_shadow_term(vec4(pos_cws, 1.0), normal, sampled_shadow_layer);
        }*/

        if (bool(conf.snow_settings_angle.x)) {
            // note: for now we use fragment snow with overlays (trajectories on top of fragment snow)
            //   for precomputed snow, we would want to disable fragment shader snow in the area where overlay tiles are available
            //   for this behavior, comment in the following if  
            
            //if (tile_dist >= 0.0f) { // -1 if snow is already calculated in tile stage
            let overlay_color: vec4f = overlay_snow(normal, pos_ws, conf.snow_settings_angle, conf.snow_settings_alt);
            material_light_response.z += conf.snow_settings_alt.w * overlay_color.a;
            albedo = mix(albedo, overlay_color.rgb, overlay_color.a);
            //}
        }

        // NOTE: PRESHADING OVERLAY ONLY APPLIED ON TILES NOT ON BACKGROUND!!!
        if (!bool(conf.overlay_postshading_enabled)) {
            var overlay_color = vec4f(0.0);
            if (conf.overlay_mode == 100u) {
                overlay_color = vec4f(normal * 0.5 + 0.5, 1.0);
            } else if (conf.overlay_mode == 101u) {
                //TODO implement
                //overlay_color = overlay_steepness(normal, dist);
            } else if (conf.overlay_mode == 102u) {
                //TODO implement
                //overlay_color = vec4f(amb_occlusion, amb_occlusion, amb_occlusion, 1.0);
            } else if (conf.overlay_mode == 103u) {
                //TODO implement
                //overlay_color = vec4(color_from_id_hash(uint(sampled_shadow_layer)), 1.0);
            } else if ((conf.overlay_mode >= 1u) || (conf.overlay_mode <= 99u)) {
                overlay_color = unpack4x8unorm(textureLoad(overlay_texture, tci, 0).r);
            }
            overlay_color.a *= conf.overlay_strength;
            albedo = mix(albedo, overlay_color.rgb, overlay_color.a);

            if (dist > 0.0 && all(pos_ws.xy >= compute_overlay_settings.aabb_min) && all(pos_ws.xy <= compute_overlay_settings.aabb_max)) {
                albedo = mix(albedo.rgb, compute_overlay_color.rgb, compute_overlay_color.a * compute_overlay_settings.alpha);
            }
        }

        var shaded_color = albedo;
        if (bool(conf.phong_enabled)) {
            shaded_color = calculate_illumination(shaded_color, origin, pos_ws, normal, conf.sun_light, conf.amb_light, conf.sun_light_dir.xyz, material_light_response, amb_occlusion, shadow_term);
        }
        if (bool(conf.atmosphere_enabled)) {
            shaded_color = calculate_atmospheric_light(origin / 1000.0, ray_direction, dist / 1000.0, shaded_color, 10);
        }
        shaded_color = max(vec3(0.0), shaded_color);
        if (dist > 0 && bool(conf.atmosphere_enabled)) {
            let atmosphere_blend = calculate_falloff(dist, 300000.0, 600000.0);
            shaded_color = mix(atmospheric_color, shaded_color, atmosphere_blend);
        }
        out_Color = vec4(shaded_color, 1.0);
    } else {
        out_Color = vec4(atmospheric_color, 1.0);
    }



    if (dist > 0.0 && all(pos_ws.xy >= image_overlay_settings.aabb_min) && all(pos_ws.xy <= image_overlay_settings.aabb_max)) {
        if (image_overlay_settings.mode == 0u) {
            out_Color = vec4f(mix(out_Color.rgb, image_overlay_color.rgb, image_overlay_color.a * image_overlay_settings.alpha), out_Color.a);
        } else if (image_overlay_settings.mode == 1u) {
            let decoded_float_value = decode_rgba_to_normalized_value(image_overlay_color.rgba);
            let encoded_float_value = (decoded_float_value - image_overlay_settings.float_decoding_lower_bound) / (image_overlay_settings.float_decoding_upper_bound - image_overlay_settings.float_decoding_lower_bound);
            if (encoded_float_value > 0.0 && encoded_float_value < 1.0) {
                let overlay_color = vec3(1.0 - encoded_float_value, 0, 0) ;
                out_Color = vec4f(mix(out_Color.rgb, overlay_color , image_overlay_settings.alpha), out_Color.a);
            }  
        }
    }

    if (bool(conf.overlay_postshading_enabled)) {
        var overlay_color = vec4f(0.0);
        if (conf.overlay_mode == 100u) {
            overlay_color = vec4f(normal * 0.5 + 0.5, 1.0);
        } else if (conf.overlay_mode == 101u) {
            //TODO implement
            //overlay_color = overlay_steepness(normal, dist);
        } else if (conf.overlay_mode == 102u) {
            //TODO implement
            //overlay_color = vec4f(amb_occlusion, amb_occlusion, amb_occlusion, 1.0);
        } else if (conf.overlay_mode == 103u) {
            //TODO implement
            //overlay_color = vec4(color_from_id_hash(uint(sampled_shadow_layer)), 1.0);
        } else if ((conf.overlay_mode >= 1u) || (conf.overlay_mode <= 99u)) {
            overlay_color = unpack4x8unorm(textureLoad(overlay_texture, tci, 0).r);
        }
        overlay_color.a *= conf.overlay_strength;
        out_Color = vec4(mix(out_Color.rgb, overlay_color.rgb, overlay_color.a), out_Color.a);

        if (dist > 0.0 && all(pos_ws.xy >= compute_overlay_settings.aabb_min) && all(pos_ws.xy <= compute_overlay_settings.aabb_max)) {
            out_Color = vec4(mix(out_Color.rgb, compute_overlay_color.rgb, compute_overlay_color.a * compute_overlay_settings.alpha), out_Color.a);
        }
    }

    // == HEIGHT LINES ==============
    if (bool(conf.height_lines_enabled) && dist > 0.0) {
        var draw_line = true; // ensures that we only darken the pixel once
        apply_height_lines(&out_Color, pos_ws, normal, dist, conf.height_lines_settings.x, conf.height_lines_settings.z, conf.height_lines_settings.w, &draw_line, 1.0);
        apply_height_lines(&out_Color, pos_ws, normal, dist, conf.height_lines_settings.y, conf.height_lines_settings.z * 0.5, conf.height_lines_settings.w * 0.75, &draw_line, 1.0);
    }

    // Clouds
    if (bool(conf.clouds_enabled)) {
        let clouds_color = textureLoad(clouds_texture, tci, 0);
        let clouds_depth = textureLoad(clouds_depth_texture, tci/2).x;

        // convert transmittance to alpha
        let raw_alpha = 1.0 - clouds_color.a;
        let safe_alpha = max(raw_alpha, 0.00001);
        let straight_rgb = clouds_color.rgb / safe_alpha;
        var tonemapped_rgb = straight_rgb / (straight_rgb + 1.0);

        // atmosphere
        if (clouds_depth > 0.0 && bool(conf.atmosphere_enabled)) {
            let atmosphere_blend = calculate_falloff(clouds_depth, 300000.0, 600000.0);
            tonemapped_rgb = mix(atmospheric_color, tonemapped_rgb, atmosphere_blend);
        }

        var blend_alpha = raw_alpha;

        out_Color = vec4(
            out_Color.rgb * (1.0 - blend_alpha) + tonemapped_rgb * blend_alpha,
            1.0 - (1.0 - out_Color.a) * (1.0 - blend_alpha)
        );
    }

    return out_Color;
}

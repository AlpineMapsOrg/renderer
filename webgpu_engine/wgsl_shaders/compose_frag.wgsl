/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2022 Gerald Kimmersdorfer
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

#include "shared_config.wgsl"
#include "camera_config.wgsl"
#include "screen_pass_shared.wgsl"
#include "atmosphere_implementation.wgsl"

@group(0) @binding(0) var<uniform> conf : shared_config;
@group(1) @binding(0) var<uniform> camera : camera_config;

@group(2) @binding(0) var albedo_texture : texture_2d<f32>;
@group(2) @binding(1) var position_texture : texture_2d<f32>;
@group(2) @binding(2) var normal_texture : texture_2d<u32>;
@group(2) @binding(3) var atmosphere_texture : texture_2d<f32>;
@group(2) @binding(4) var compose_sampler_filtering : sampler;
@group(2) @binding(5) var compose_sampler_nonfiltering : sampler;


fn calculate_falloff(dist: f32, start: f32, end: f32) -> f32 {
    return clamp(1.0 - (dist - start) / (end - start), 0.0, 1.0);
}

@fragment
fn fragmentMain(vertex_out : VertexOut) -> @location(0) vec4f {
    let albedo = textureSample(albedo_texture, compose_sampler_filtering, vertex_out.texcoords.xy).rgb;

    let pos_dist = textureSample(position_texture, compose_sampler_nonfiltering, vertex_out.texcoords.xy);
    let pos_cws = pos_dist.xyz;
    let dist = pos_dist.w;
    var alpha = 0.0;
    if (dist > 0) {
        alpha = calculate_falloff(dist, 300000.0, 600000.0);
    }

    //TODO
    //let normal = octNormalDecode2u16(texture(texin_normal, texcoords).xy); //vec3f

    var shaded_color = vec3f(0.0);
    
    var amb_occlusion = 1.0;
    // Gather ambient occlusion from ssao texture
    if (bool(conf.ssao_enabled)) {
        //TODO
        //amb_occlusion = texture(texin_ssao, texcoords).r;
    }
    
    let sampled_shadow_layer: i32 = -1;

    // Don't do shading if not visible anyway and also don't for pixels where there is no geometry (depth==0.0)
    if (dist > 0.0) {
        let origin = camera.position.xyz;
        let pos_ws = pos_cws + origin;
        let ray_direction = pos_cws / dist;
        let material_light_response = conf.material_light_response;

        //TODO (unused in original code?)
        //let light_through_atmosphere = calculate_atmospheric_light(origin / 1000.0, ray_direction, dist / 1000.0, albedo, 10);

        var shadow_term = 0.0;
        if (bool(conf.csm_enabled)) {
            //TODO
            //shadow_term = csm_shadow_term(vec4(pos_cws, 1.0), normal, sampled_shadow_layer);
        }

        if (bool(conf.snow_settings_angle.x)) {
            //TODO
            //lowp vec4 overlay_color = overlay_snow(normal, pos_ws, dist);
            //material_light_response.z += conf.snow_settings_alt.w * overlay_color.a;
            //albedo = mix(albedo, overlay_color.rgb, overlay_color.a);
        }

        // NOTE: PRESHADING OVERLAY ONLY APPLIED ON TILES NOT ON BACKGROUND!!!
        if (!bool(conf.overlay_postshading_enabled) && conf.overlay_mode >= 100u) {
            //TODO
            //lowp vec4 overlay_color = vec4(0.0);
            //switch(conf.overlay_mode) {
            //    case 100u: overlay_color = vec4(normal * 0.5 + 0.5, 1.0); break;
            //    case 101u: overlay_color = overlay_steepness(normal, dist); break;
            //    case 102u: overlay_color = vec4(vec3(amb_occlusion), 1.0); break;
            //    case 103u: overlay_color = vec4(color_from_id_hash(uint(sampled_shadow_layer)), 1.0); break;
            //}
            //overlay_color.a *= conf.overlay_strength;
            //albedo = mix(albedo, overlay_color.rgb, overlay_color.a);
        }

        shaded_color = albedo;
        if (bool(conf.phong_enabled)) {
            //TODO normal
            //shaded_color = calculate_illumination(shaded_color, origin, pos_ws, normal, conf.sun_light, conf.amb_light, conf.sun_light_dir.xyz, material_light_response, amb_occlusion, shadow_term);
        }
        shaded_color = calculate_atmospheric_light(origin / 1000.0, ray_direction, dist / 1000.0, shaded_color, 10);
        shaded_color = max(vec3(0.0), shaded_color);    
    }

    // Blend with atmospheric background:
    let atmospheric_color = textureSample(atmosphere_texture, compose_sampler_filtering, vertex_out.texcoords.xy).rgb;
    var out_Color = vec4f(mix(atmospheric_color, shaded_color, alpha), 1.0);

    //TODO
    /*

    if (bool(conf.overlay_postshading_enabled) && conf.overlay_mode >= 100u) {
        lowp vec4 overlay_color = vec4(0.0);
        switch(conf.overlay_mode) {
            case 100u: overlay_color = vec4(normal * 0.5 + 0.5, 1.0); break;
            case 101u: overlay_color = overlay_steepness(normal, dist); break;
            case 102u: overlay_color = vec4(vec3(amb_occlusion), 1.0); break;
            case 103u: overlay_color = vec4(color_from_id_hash(uint(sampled_shadow_layer)), 1.0); break;
        }
        overlay_color.a *= conf.overlay_strength;
        out_Color = vec4(mix(out_Color.rgb, overlay_color.rgb, overlay_color.a), out_Color.a);
    }

    // OVERLAY SHADOW MAPS
    if (bool(conf.overlay_shadowmaps_enabled)) {
        highp float wsize = 1.0 / float(SHADOW_CASCADES);
        highp float invwsize = 1.0/wsize;
        if (texcoords.x < wsize) {
            for (int i = 0 ; i < SHADOW_CASCADES; i++)
            {
                if (texcoords.y < wsize * float(i+1)) {
                    highp float val = sample_shadow_texture(i, (texcoords - vec2(0.0, wsize*float(i))) * invwsize);
                    out_Color = vec4(val, val, val, 1.0);
                    break;
                }
            }
        }
    }

    // == HEIGHT LINES ==============
    if (bool(conf.height_lines_enabled) && dist > 0.0) {
        highp float alpha_line = 1.0 - min((dist / 20000.0), 1.0);
        highp float line_width = (2.0 + dist / 5000.0) * 5.0;
        // Calculate steepness based on fragment normal (this alone gives woobly results)
        highp float steepness = (1.0 - dot(normal, vec3(0.0,0.0,1.0))) / 2.0;
        // Discretize the steepness -> Doesnt work
        //float steepness_discretized = int(steepness * 10.0f) / 10.0f;
        line_width = line_width * max(0.01,steepness);
        if (alpha_line > 0.05)
        {
            highp float alt = pos_cws.z + camera.position.z;
            highp float alt_rest = (alt - float(int(alt / 100.0)) * 100.0) - line_width / 2.0;
            if (alt_rest < line_width) {
                out_Color = vec4(mix(out_Color.rgb, vec3(out_Color.r - 0.2, out_Color.g - 0.2, out_Color.b - 0.2), alpha_line), out_Color.a);
            }
        }
    }

    */

    return out_Color;
}

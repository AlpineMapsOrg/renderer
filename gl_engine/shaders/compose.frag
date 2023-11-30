/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Gerald Kimmersdorfer
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

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

#include "atmosphere_implementation.glsl"
#include "encoder.glsl"
#include "shared_config.glsl"
#include "shadow_config.glsl"
#include "camera_config.glsl"
#include "hashing.glsl"
#include "overlay_steepness.glsl"
#include "snow.glsl"

layout (location = 0) out lowp vec4 out_Color;

in highp vec2 texcoords;


uniform sampler2D texin_albedo;             // 8vec3
uniform highp sampler2D texin_position;     // f32vec4
uniform highp usampler2D texin_normal;      // u16vec2

uniform sampler2D texin_atmosphere;         // 8vec3
uniform sampler2D texin_ssao;               // 8vec1

uniform highp sampler2D texin_csm1;         // f32vec1
uniform highp sampler2D texin_csm2;         // f32vec1
uniform highp sampler2D texin_csm3;         // f32vec1
uniform highp sampler2D texin_csm4;         // f32vec1


// Calculates the diffuse and specular illumination contribution for the given
// parameters according to the Blinn-Phong lighting model.
// All parameters must be normalized.
highp vec3 calc_blinn_phong_contribution(highp vec3 toLight, highp vec3 toEye, highp vec3 normal, highp vec3 diffFactor, highp vec3 specFactor, highp float specShininess)
{
    highp float nDotL = max(0.0, dot(normal, toLight)); // lambertian coefficient
    highp vec3 h = normalize(toLight + toEye);
    highp float nDotH = max(0.0, dot(normal, h));
    highp float specPower = pow(nDotH, specShininess);
    highp vec3 diffuse = diffFactor * nDotL; // component-wise product
    highp vec3 specular = specFactor * specPower;
    return diffuse + specular;
}

// Calculates the blinn phong illumination for the given fragment
highp vec3 calculate_illumination(highp vec3 albedo, highp vec3 eyePos, highp vec3 fragPos, highp vec3 fragNorm, highp vec4 dirLight, highp vec4 ambLight, highp vec3 dirDirection, highp vec4 material, highp float ao, highp float shadow_term) {
    highp vec3 dirColor = dirLight.rgb * dirLight.a;
    highp vec3 ambColor = ambLight.rgb * ambLight.a;
    highp vec3 ambient = material.r * albedo;
    highp vec3 diff = material.g * albedo;
    highp vec3 spec = vec3(material.b);
    highp float shini = material.a;

    highp vec3 ambientIllumination = ambient * ambColor * ao;

    highp vec3 toLightDirWS = -normalize(dirDirection);
    highp vec3 toEyeNrmWS = normalize(eyePos - fragPos);
    highp vec3 diffAndSpecIllumination = dirColor * calc_blinn_phong_contribution(toLightDirWS, toEyeNrmWS, fragNorm, diff, spec, shini);

    return ambientIllumination + diffAndSpecIllumination * (1.0 - shadow_term);
}

// Because of limited support I didn't use texture arrays, therefore
// we need this selector function to sample from a specified shadowmap level
highp float sample_shadow_texture(lowp int layer, highp vec2 texcoords) {
    switch (layer) {
        case 0: return texture(texin_csm1, texcoords).r;
        case 1: return texture(texin_csm2, texcoords).r;
        case 2: return texture(texin_csm3, texcoords).r;
        case 3: return texture(texin_csm4, texcoords).r;
        default: return 0.0;
    }
}

highp float csm_shadow_term(highp vec4 pos_cws, highp vec3 normal_ws, out lowp int layer) {
    // SELECT LAYER
    highp vec4 pos_vs = camera.view_matrix * pos_cws;
    highp float depth_cam = abs(pos_vs.z);

    layer = -1;
    for (lowp int i = 0; i < SHADOW_CASCADES; i++) {
        if (depth_cam < shadow.cascade_planes[i + 1]) {
            layer = i;
            break;
        }
    }

    highp float depth_fallof_from = shadow.cascade_planes[SHADOW_CASCADES - 1] + (shadow.cascade_planes[SHADOW_CASCADES - 0] - shadow.cascade_planes[SHADOW_CASCADES - 1]) / 2.0;
    highp float depth_fallof_to = shadow.cascade_planes[SHADOW_CASCADES - 0];
    highp float alpha = calculate_falloff(depth_cam, depth_fallof_from, depth_fallof_to);

    highp vec4 pos_ls = shadow.light_space_view_proj_matrix[layer] * pos_cws;
    highp vec3 pos_ls_ndc = pos_ls.xyz / pos_ls.w * 0.5 + 0.5;

    highp float depth_ls = pos_ls_ndc.z;
    //if (depth_ls > 1.0) return 0.0; //not necessary because orthogonal

    // calculate bias based on depth resolution and slope
    highp float bias = max(0.05 * (1.0 - dot(normal_ws, -conf.sun_light_dir.xyz)), 0.005);
    highp float dist = length(pos_cws.xyz);
    highp float biasModifier = 1.0;

    if (layer == 0) {
        biasModifier = 1.0 / dist * 50.0;
    } else if (layer == 1) {
        biasModifier = 1.0 / dist * 50.0;
    } else if (layer == 2) {
        biasModifier = 1.0 / dist * 25.0;
    } else if (layer == 3) {
        biasModifier = 1.0 / dist * 15.0;
    }
    if (dist < 500.0) biasModifier = biasModifier / 10.0;
    //biasModifier = 0.005;
    bias *= 1.0 / (shadow.cascade_planes[layer + 1] * biasModifier);

    highp float term = 0.0;
    highp vec2 texelSize = 1.0 / shadow.shadowmap_size;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            highp float pcfDepth = sample_shadow_texture(layer, pos_ls_ndc.xy + vec2(x,y) * texelSize);
            term += (depth_ls - bias) > pcfDepth ? 1.0 : 0.0;
        }
    }
    term /= 9.0;
    return mix(term, 1.0, 1.0-alpha);
}

void main() {
    lowp vec3 albedo = texture(texin_albedo, texcoords).rgb;

    highp vec4 pos_dist = texture(texin_position, texcoords);
    highp vec3 pos_cws = pos_dist.xyz;
    highp float dist = pos_dist.w; // negative if sky
    // Alpha-Value for Tile-Overlay (distant linear falloff)
    lowp float alpha = 0.0;
    if (dist > 0.0) alpha = calculate_falloff(dist, 300000.0, 600000.0);

    highp vec3 normal = octNormalDecode2u16(texture(texin_normal, texcoords).xy);

    highp vec3 shaded_color = vec3(0.0f);
    highp float amb_occlusion = 1.0;
    // Gather ambient occlusion from ssao texture
    if (bool(conf.ssao_enabled)) amb_occlusion = texture(texin_ssao, texcoords).r;

    lowp int sampled_shadow_layer = -1;

    // Don't do shading if not visible anyway and also don't for pixels where there is no geometry (depth==0.0)
    if (dist > 0.0) {
        highp vec3 origin = vec3(camera.position);
        highp vec3 pos_ws = pos_cws + origin;
        highp vec3 ray_direction = pos_cws / dist;
        highp vec4 material_light_response = conf.material_light_response;

        highp vec3 light_through_atmosphere = calculate_atmospheric_light(origin / 1000.0, ray_direction, dist / 1000.0, albedo, 10);

        highp float shadow_term = 0.0;
        if (bool(conf.csm_enabled)) {
            shadow_term = csm_shadow_term(vec4(pos_cws, 1.0), normal, sampled_shadow_layer);
        }

        if (bool(conf.snow_settings_angle.x)) {
            lowp vec4 overlay_color = overlay_snow(normal, pos_ws, dist);
            material_light_response.z += conf.snow_settings_alt.w * overlay_color.a;
            albedo = mix(albedo, overlay_color.rgb, overlay_color.a);
        }

        // NOTE: PRESHADING OVERLAY ONLY APPLIED ON TILES NOT ON BACKGROUND!!!
        if (!bool(conf.overlay_postshading_enabled) && conf.overlay_mode >= 100u) {
            lowp vec4 overlay_color = vec4(0.0);
            switch(conf.overlay_mode) {
                case 100u: overlay_color = vec4(normal * 0.5 + 0.5, 1.0); break;
                case 101u: overlay_color = overlay_steepness(normal, dist); break;
                case 102u: overlay_color = vec4(vec3(amb_occlusion), 1.0); break;
                case 103u: overlay_color = vec4(color_from_id_hash(uint(sampled_shadow_layer)), 1.0); break;
            }
            overlay_color.a *= conf.overlay_strength;
            albedo = mix(albedo, overlay_color.rgb, overlay_color.a);
        }

        shaded_color = albedo;
        if (bool(conf.phong_enabled)) {
            shaded_color = calculate_illumination(shaded_color, origin, pos_ws, normal, conf.sun_light, conf.amb_light, conf.sun_light_dir.xyz, material_light_response, amb_occlusion, shadow_term);
        }
        shaded_color = calculate_atmospheric_light(origin / 1000.0, ray_direction, dist / 1000.0, shaded_color, 10);
        shaded_color = max(vec3(0.0), shaded_color);
    }

    // Blend with atmospheric background:
    lowp vec3 atmoshperic_color = texture(texin_atmosphere, texcoords).rgb;
    out_Color = vec4(mix(atmoshperic_color, shaded_color, alpha), 1.0);

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

}

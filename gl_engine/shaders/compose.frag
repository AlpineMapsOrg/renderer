/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include "atmosphere_implementation.glsl"
#include "encoder.glsl"
#include "shared_config.glsl"
#include "shadow_config.glsl"
#include "camera_config.glsl"
#include "hashing.glsl"

in highp vec2 texcoords;

uniform sampler2D texin_albedo;
uniform lowp sampler2D texin_depthold;
uniform highp sampler2D texin_normalold;

uniform highp sampler2D texin_position;
uniform sampler2D texin_atmosphere;
uniform sampler2D texin_ssao;
uniform highp sampler2D texin_depth;

uniform highp usampler2D texin_normal;

uniform highp sampler2D texin_csm1;
uniform highp sampler2D texin_csm2;
uniform highp sampler2D texin_csm3;
uniform highp sampler2D texin_csm4;

layout (location = 0) out lowp vec4 out_Color;

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

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
    highp vec3 spec = material.bbb;
    highp float shini = material.a;

    highp vec3 ambientIllumination = ambient * ambColor * ao;

    highp vec3 toLightDirWS = -normalize(dirDirection);
    highp vec3 toEyeNrmWS = normalize(eyePos - fragPos);
    highp vec3 diffAndSpecIllumination = dirColor * calc_blinn_phong_contribution(toLightDirWS, toEyeNrmWS, fragNorm, diff, spec, shini);

    return ambientIllumination + diffAndSpecIllumination * (1.0 - shadow_term);
}

highp float sample_shadow_texture(lowp int layer, highp vec2 texcoords) {
    switch (layer) {
        case 0: return texture(texin_csm1, texcoords).r;
        case 1: return texture(texin_csm2, texcoords).r;
        case 2: return texture(texin_csm3, texcoords).r;
        case 3: return texture(texin_csm4, texcoords).r;
        default: return 0.0;
    }
}

highp vec3 debug_color = vec3(0.0);

highp float csm_shadow_term(highp vec4 pos_cws, highp vec3 normal_ws) {
    // SELECT LAYER
    highp vec4 pos_vs = camera.view_matrix * pos_cws;
    highp float depth_cam = abs(pos_vs.z);

    lowp int layer = -1;
    for (lowp int i = 0; i < SHADOW_CASCADES; i++) {
        if (depth_cam < shadow.cascade_planes[i + 1]) {
            layer = i;
            break;
        }
    }
    if (layer == -1) layer = SHADOW_CASCADES - 1;

    debug_color = color_from_id_hash(uint(layer));

    highp vec4 pos_ls = shadow.light_space_view_proj_matrix[layer] * pos_cws;
    highp vec3 pos_ls_ndc = pos_ls.xyz / pos_ls.w * 0.5 + 0.5;

    highp float depth_ls = pos_ls_ndc.z;
    if (depth_ls > 1.0) return 0.0;

    // calculate bias based on depth resolution and slope
    highp float bias = max(0.05 * (1.0 - dot(normal_ws, -conf.sun_light_dir.xyz)), 0.005); // ToDo: Make sure - is correct

    highp float dist = length(pos_cws.xyz);
    highp float biasModifier = 1.0 / dist * 10.0; // make depth dependent
    biasModifier = 0.005;
    bias *= 1.0 / (shadow.cascade_planes[layer + 1] * biasModifier);
    //bias = 0.0005;

    highp float term = 0.0;
    highp vec2 texelSize = 1.0 / shadow.shadowmap_size;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            highp float pcfDepth = sample_shadow_texture(layer, pos_ls_ndc.xy + vec2(x,y) * texelSize);
            term += (depth_ls - bias) > pcfDepth ? 1.0 : 0.0;
        }
    }
    term /= 9.0;
    return term;
}

void main() {
    lowp vec4 albedo = texture(texin_albedo, texcoords).rgb;


    // Reconstruct position from depth buffer
    highp float depth_cs = texture(texin_depth, texcoords).x;
    highp vec3 pos_wrt_cam = vec3(0.0);
    highp float dist = -1.0;        // Distance to camera
    lowp float alpha = 0.0;         // Alpha-Value for Tile-Overlay (distant linear falloff)
    if (depth_cs != FARPLANE_DEPTH_VALUE) {
        pos_wrt_cam = depth_cs_to_pos_ws(depth_cs, texcoords);
        dist = length(pos_wrt_cam);
        alpha = calculate_falloff(dist, 300000.0, 600000.0);
    }

    highp vec3 normal = octNormalDecode2u16(texture(texin_normal, texcoords).xy);

    highp vec3 shaded_color = vec3(0.0f);

    // Don't do shading if not visible anyway and also don't for pixels where there is no geometry (depth==0.0)
    bool do_shading = (conf.debug_overlay_strength < 1.0f || conf.debug_overlay == 0u) && depth_cs != FARPLANE_DEPTH_VALUE;
    if (do_shading) {
        highp vec3 origin = vec3(camera.position);

        highp vec3 ray_direction = pos_wrt_cam / dist;

        highp vec3 light_through_atmosphere = calculate_atmospheric_light(origin / 1000.0, ray_direction, dist / 1000.0, albedo, 10);

        highp float shadow_term = 0.0;
        if (bool(conf.csm_enabled)) {
            shadow_term = csm_shadow_term(vec4(pos_wrt_cam, 1.0), normal);
        }

        // Gather ambient occlusion from ssao texture
        highp float amb_occlusion = 1.0;
        if (bool(conf.ssao_enabled)) amb_occlusion = texture(texin_ssao, texcoords).r;

        shaded_color = albedo;
        if (bool(conf.phong_enabled)) {
            shaded_color = calculate_illumination(shaded_color, origin, pos_wrt_cam, normal, conf.sun_light, conf.amb_light, conf.sun_light_dir.xyz, conf.material_light_response, amb_occlusion, shadow_term);
        }
        shaded_color = calculate_atmospheric_light(origin / 1000.0, ray_direction, dist / 1000.0, shaded_color, 10);
        shaded_color = max(vec3(0.0), shaded_color);
    }

    // Blend with atmospheric background:
    lowp vec3 atmoshperic_color = texture(texin_atmosphere, texcoords).rgb;
    out_Color = vec4(mix(atmoshperic_color, shaded_color, alpha), 1.0);

    if (conf.debug_overlay_strength > 0.0 && conf.debug_overlay > 0u) {
        highp vec4 overlayColor = vec4(-1.0);
        if (overlayColor.x > 0) out_Color = mix(out_Color, overlayColor, conf.debug_overlay_strength);
    }

    // OVERLAY SHADOW MAPS
    if (bool(conf.overlay_shadowmaps)) {
        highp float wsize = 0.25;
        highp float invwsize = 1.0/wsize;
        if (texcoords.x < wsize) {
            if (texcoords.y < wsize) {
                out_Color = texture(texin_csm1, (texcoords - vec2(0.0, wsize*0.0)) * invwsize).rrrr;
            } else if (texcoords.y < wsize * 2.0) {
                out_Color = texture(texin_csm2, (texcoords - vec2(0.0, wsize*1.0)) * invwsize).rrrr;
            } else if (texcoords.y < wsize * 3.0) {
                out_Color = texture(texin_csm3, (texcoords - vec2(0.0, wsize*2.0)) * invwsize).rrrr;
            } else if (texcoords.y < wsize * 4.0) {
                out_Color = texture(texin_csm4, (texcoords - vec2(0.0, wsize*3.0)) * invwsize).rrrr;
            }
        }
    }
}

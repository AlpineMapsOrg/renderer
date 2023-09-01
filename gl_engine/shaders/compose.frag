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

in highp vec2 texcoords;

uniform highp vec3 camera_position;
uniform highp mat4 inv_view_projection_matrix;

uniform sampler2D texin_albedo;
uniform sampler2D texin_depth;
uniform sampler2D texin_normal;
uniform sampler2D texin_position;
uniform sampler2D texin_atmosphere;
uniform sampler2D texin_ssao;

layout (location = 0) out lowp vec4 out_Color;

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

// Calculates the diffuse and specular illumination contribution for the given
// parameters according to the Blinn-Phong lighting model.
// All parameters must be normalized.
vec3 calc_blinn_phong_contribution(vec3 toLight, vec3 toEye, vec3 normal, vec3 diffFactor, vec3 specFactor, float specShininess)
{
    float nDotL = max(0.0, dot(normal, toLight)); // lambertian coefficient
    vec3 h = normalize(toLight + toEye);
    float nDotH = max(0.0, dot(normal, h));
    float specPower = pow(nDotH, specShininess);
    vec3 diffuse = diffFactor * nDotL; // component-wise product
    vec3 specular = specFactor * specPower;
    return diffuse + specular;
}

// Calculates the blinn phong illumination for the given fragment
vec3 calculate_illumination(vec3 albedo, vec3 eyePos, vec3 fragPos, vec3 fragNorm, vec4 dirLight, vec4 ambLight, vec3 dirDirection, vec4 material, float ao) {
    vec3 dirColor = dirLight.rgb * dirLight.a;
    vec3 ambColor = ambLight.rgb * ambLight.a;
    vec3 ambient = material.r * albedo;
    vec3 diff = material.g * albedo;
    vec3 spec = material.bbb;
    float shini = material.a;

    vec3 ambientIllumination = ambient * ambColor * ao;

    vec3 toLightDirWS = -normalize(dirDirection);
    vec3 toEyeNrmWS = normalize(eyePos - fragPos);
    vec3 diffAndSpecIllumination = dirColor * calc_blinn_phong_contribution(toLightDirWS, toEyeNrmWS, fragNorm, diff, spec, shini);

    return ambientIllumination + diffAndSpecIllumination;
}

void main() {
    lowp vec4 albedo_alpha = texture(texin_albedo, texcoords);
    lowp vec3 albedo = albedo_alpha.rgb;
    lowp float alpha = albedo_alpha.a;
    lowp vec2 depth_encoded = texture(texin_depth, texcoords).xy;
    highp vec4 normal_dist = texture(texin_normal, texcoords);
    highp vec3 pos_wrt_cam = texture(texin_position, texcoords).xyz;
    highp float ssao = texture(texin_ssao, texcoords).r;
    highp vec3 normal = normal_dist.xyz;
    highp float dist = normal_dist.w;

    // DECODE DEPTH
    //highp float depth_decoded = decode(depth_encoded);
    //highp float dist_decoded = exp(depth_decoded * 13.0f);
    //highp float dist = exp(depth_true * 13.0f);

    highp vec3 shaded_color = vec3(0.0f);

    if (conf.debug_overlay_strength < 1.0f || conf.debug_overlay == 0u) {
        highp vec3 origin = vec3(camera_position);

        //highp float dist = length(pos_wrt_cam);
        highp vec3 ray_direction = pos_wrt_cam / dist;

        highp vec3 light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, albedo, 10);

        shaded_color = albedo;
        if (conf.phong_enabled) {
            shaded_color = calculate_illumination(shaded_color, origin, pos_wrt_cam, normal, conf.sun_light, conf.amb_light, conf.sun_light_dir.xyz, conf.material_light_response, ssao);
        }
        shaded_color = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, shaded_color, 10);
        shaded_color = max(vec3(0.0), shaded_color);
    }

    // Blend with atmospheric background:
    // Note: Alpha value gets calculated in gbuffer stage since the depth doesnt seem
    // to be accurate enough at such far distances!
    lowp vec3 atmoshperic_color = texture(texin_atmosphere, texcoords).rgb;
    out_Color = vec4(mix(atmoshperic_color, shaded_color, alpha), 1.0);

    out_Color = vec4(vec3(ssao), 1.0);

    //out_Color = vec4(atmoshperic_color * (1.0 - alpha),1.0);
    //out_Color = vec4(vec3(alpha), 1.0);

    // == HEIGHT LINES ==============
/*
    float alpha = 1.0 - min((dist / 10000.0), 1.0);
    float line_width = (2.0 + dist / 5000.0) * 5.0;
    // Calculate steepness based on fragment normal (this alone gives woobly results)
    float steepness = (1.0 - dot(normal, vec3(0.0,0.0,1.0))) / 2.0;
    // Discretize the steepness -> Doesnt work
    //float steepness_discretized = int(steepness * 10.0f) / 10.0f;
    line_width = line_width * max(0.01,steepness);
    if (alpha > 0.05)
    {
        float alt = pos_wrt_cam.z + camera_position.z;
        float alt_rest = (alt - int(alt / 100.0) * 100.0) - line_width / 2.0;
        if (alt_rest < line_width) {
            out_Color = mix(out_Color, vec4(out_Color.r - 0.2, out_Color.g - 0.2, out_Color.b - 0.2, 1.0), alpha);
        }
    }*/

    //out_Color = vec4(albedo * pos_wrt_cam, 1.0) * normal_dist * depth_encoded.x;
    //if (abs(dist_decoded - dist) > 1.0) {
    //    out_Color = vec4(1.0, 0.0, 0.0,1.0);
    //}
}

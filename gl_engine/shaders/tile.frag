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

uniform highp vec3 camera_position;

uniform sampler2D texture_sampler;

layout (std140) uniform shared_config {
    vec4 sun_light;
    vec4 sun_light_dir;
    vec4 amb_light;
    vec4 material_color;
    vec4 material_light_response;
    vec4 curtain_settings;
    bool phong_enabled;
    uint wireframe_mode;
    uint normal_mode;
    uint debug_overlay;
    float debug_overlay_strength;
} conf;

layout (location = 0) out lowp vec4 out_Color;
layout (location = 1) out lowp vec4 out_Depth;

in lowp vec2 uv;
in highp vec3 pos_wrt_cam;
in highp vec3 var_normal;
in float is_curtain;
//flat in vec3 vertex_color;
in vec3 vertex_color;
in float drop_frag;
in vec3 debug_overlay_color;



highp vec3 calculate_atmospheric_light(highp vec3 ray_origin, highp vec3 ray_direction, highp float ray_length, highp vec3 original_colour, int n_numerical_integration_steps);

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
vec3 calculate_illumination(vec3 albedo, vec3 eyePos, vec3 fragPos, vec3 fragNorm) {
    vec4 mMaterialLightReponse = conf.material_light_response;
    vec3 dirColor = conf.sun_light.rgb * conf.sun_light.a;
    vec3 ambient = mMaterialLightReponse.x * albedo;
    vec3 diff = mMaterialLightReponse.y * albedo;
    vec3 spec = mMaterialLightReponse.zzz;
    float shini = mMaterialLightReponse.w;

    vec3 ambientIllumination = ambient * conf.amb_light.rgb * conf.amb_light.a; // The color of the ambient light

    vec3 toLightDirWS = -normalize(conf.sun_light_dir.xyz);
    vec3 toEyeNrmWS = normalize(eyePos - fragPos);
    vec3 diffAndSpecIllumination = dirColor * calc_blinn_phong_contribution(toLightDirWS, toEyeNrmWS, fragNorm, diff, spec, shini);

    return ambientIllumination + diffAndSpecIllumination;
}

vec3 normal_by_fragment_position_interpolation() {
    vec3 dFdxPos = dFdx(pos_wrt_cam);
    vec3 dFdyPos = dFdy(pos_wrt_cam);
    return normalize(cross(dFdxPos, dFdyPos));
}

lowp vec2 encode(highp float value) {
    mediump uint scaled = uint(value * 65535.f + 0.5f);
    mediump uint r = scaled >> 8u;
    mediump uint b = scaled & 255u;
    return vec2(float(r) / 255.f, float(b) / 255.f);
}

void main() {
    if (conf.wireframe_mode == 2u) {
        out_Color = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }
    if (is_curtain > 0) {
        if (conf.curtain_settings.x == 2.0) {
            out_Color = vec4(1.0, 0.0, 0.0, 1.0);
            return;
        } else if (conf.curtain_settings.x == 0.0) {
            discard;
        }
    } else {
        if (conf.curtain_settings.x == 3.0) {
            discard;
        }
    }

    highp float dist = length(pos_wrt_cam);
    highp float depth = log(dist)/13.0;
    out_Depth = vec4(encode(depth), 0, 0);

    highp vec4 ortho = texture(texture_sampler, uv);
    vec3 normal = vec3(0.0);

    if (conf.normal_mode == 0u) {
        normal = normal_by_fragment_position_interpolation();
    } else {
        normal = var_normal;
    }

    if (conf.debug_overlay_strength < 1.0f || conf.debug_overlay == 0u) {
        highp vec3 origin = vec3(camera_position);

        highp float dist = length(pos_wrt_cam);
        highp vec3 ray_direction = pos_wrt_cam / dist;

        highp vec3 light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, vec3(ortho), 10);
        highp float cos_f = dot(ray_direction, vec3(0.0, 0.0, 1.0));
        highp float alpha = calculate_falloff(dist, 300000.0, 600000.0);

        vec3 fragColor = ortho.rgb;
        vec3 phong_illumination = vec3(1.0);
        if (conf.phong_enabled) {
            fragColor = mix(conf.material_color.rgb, fragColor, conf.material_color.a);
            phong_illumination = calculate_illumination(fragColor, origin, pos_wrt_cam, normal);
        }
        light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, phong_illumination * fragColor, 10);
        vec3 color = light_through_atmosphere;

        out_Color = vec4(color * alpha, alpha);
    }

    if (conf.debug_overlay_strength > 0.0 && conf.debug_overlay > 0u) {
        vec4 overlayColor = vec4(0.0);
        if (conf.debug_overlay == 1u) overlayColor = ortho;
        else if (conf.debug_overlay == 2u) overlayColor = vec4(normal * 0.5 + 0.5, 1.0);
        else overlayColor = vec4(vertex_color, 1.0);
        out_Color = mix(out_Color, overlayColor, conf.debug_overlay_strength);
    }

    if (length(debug_overlay_color) > 0.0)
        out_Color = vec4(mix(debug_overlay_color, out_Color.rgb, 0.5), 1.0);

    //out_Color = vec4(uv.rg, 0.0, 1.0);
    /*if (is_curtain > 0) {
        out_Color = vec4(1.0, 0.0, 0.0, 1.0);
    }*/
}

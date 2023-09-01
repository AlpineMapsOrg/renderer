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

#include "shared_config.glsl"

uniform highp mat4 view_proj_matrix;
uniform highp mat4 view_matrix;
uniform highp mat4 proj_matrix;

uniform highp vec3 camera_position;

uniform sampler2D texture_sampler;

layout (location = 0) out lowp vec4 texout_albedo;
layout (location = 1) out lowp vec4 texout_depth;
layout (location = 2) out highp vec4 texout_normal;    // a = dist
layout (location = 3) out highp vec3 texout_position;

in lowp vec2 uv;
in highp vec3 var_pos_wrt_cam;
in highp vec3 var_pos_vs;
in highp vec3 var_normal;
in highp vec3 var_normal_vs;
in float is_curtain;
//flat in vec3 vertex_color;
in vec3 vertex_color;
in float drop_frag;
in vec3 debug_overlay_color;

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

vec3 normal_by_fragment_position_interpolation() {
    vec3 dFdxPos = dFdx(var_pos_wrt_cam);
    vec3 dFdyPos = dFdy(var_pos_wrt_cam);
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
        texout_albedo = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }
    if (is_curtain > 0) {
        if (conf.curtain_settings.x == 2.0) {
            texout_albedo = vec4(1.0, 0.0, 0.0, 1.0);
            return;
        } else if (conf.curtain_settings.x == 0.0) {
            discard;
        }
    } else {
        if (conf.curtain_settings.x == 3.0) {
            discard;
        }
    }

    texout_position = var_pos_wrt_cam;
    texout_position = var_pos_vs;

    highp float dist = length(var_pos_wrt_cam);
    highp float depth = log(dist)/13.0;
    texout_depth = vec4(encode(depth), 0, 0);


    vec3 normal = vec3(0.0);
    vec3 normal_vs = vec3(0.0);
    if (conf.normal_mode == 0u) {
        normal = normal_by_fragment_position_interpolation();
        normal_vs = vec3(view_matrix * vec4(normal, 0.0));
    } else {
        normal = var_normal;
        normal_vs = var_normal_vs;
    }
    texout_normal = vec4(normal, dist);
    texout_normal = vec4(normal_vs, dist);

    lowp vec3 fragColor = texture(texture_sampler, uv).rgb;
    fragColor = mix(conf.material_color.rgb, fragColor, conf.material_color.a);
    highp float alpha = calculate_falloff(dist, 300000.0, 600000.0);
    texout_albedo = vec4(fragColor, alpha);
    //texout_albedo = vec4(alpha);


    /*
    if (conf.debug_overlay_strength < 1.0f || conf.debug_overlay == 0u) {
        highp vec3 origin = vec3(camera_position);

        highp float dist = length(var_pos_wrt_cam);
        highp vec3 ray_direction = var_pos_wrt_cam / dist;

        highp vec3 light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, vec3(ortho), 10);
        highp float cos_f = dot(ray_direction, vec3(0.0, 0.0, 1.0));
        highp float alpha = calculate_falloff(dist, 300000.0, 600000.0);

        vec3 fragColor = ortho.rgb;
        vec3 phong_illumination = vec3(1.0);
        if (conf.phong_enabled) {
            fragColor = mix(conf.material_color.rgb, fragColor, conf.material_color.a);
            phong_illumination = calculate_illumination(fragColor, origin, var_pos_wrt_cam, normal, conf.sun_light, conf.amb_light, conf.sun_light_dir.xyz, conf.material_light_response);
        }
        light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, phong_illumination * fragColor, 10);
        vec3 color = light_through_atmosphere;

        texout_albedo = vec4(color * alpha, alpha);
    }

    vec3 d_color = debug_overlay_color;

    if (conf.debug_overlay_strength > 0.0 && conf.debug_overlay > 0u) {
        vec4 overlayColor = vec4(0.0);
        if (conf.debug_overlay == 1u) overlayColor = ortho;
        else if (conf.debug_overlay == 2u) overlayColor = vec4(normal * 0.5 + 0.5, 1.0);
        else overlayColor = vec4(vertex_color, 1.0);
        texout_albedo = mix(texout_albedo, overlayColor, conf.debug_overlay_strength);
    }

    if (length(d_color) > 0.0)
        texout_albedo = vec4(d_color, 1.0);
*/
    // == HEIGHT LINES ==============
    /*float alpha = 1.0 - min((dist / 10000.0), 1.0);
    float line_width = (2.0 + dist / 5000.0) * 5.0;
    // Calculate steepness based on fragment normal (this alone gives woobly results)
    float steepness = (1.0 - dot(normal, vec3(0.0,0.0,1.0))) / 2.0;
    // Discretize the steepness -> Doesnt work
    //float steepness_discretized = int(steepness * 10.0f) / 10.0f;
    line_width = line_width * max(0.01,steepness);
    if (alpha > 0.05)
    {
        float alt = var_pos_wrt_cam.z + camera_position.z;
        float alt_rest = (alt - int(alt / 100.0) * 100.0) - line_width / 2.0;
        if (alt_rest < line_width) {
            texout_albedo = mix(texout_albedo, vec4(texout_albedo.r - 0.2, texout_albedo.g - 0.2, texout_albedo.b - 0.2, 1.0), alpha);
        }
    }*/
}

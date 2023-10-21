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
#include "camera_config.glsl"
#include "encoder.glsl"

uniform sampler2D texture_sampler;

layout (location = 0) out lowp vec4 texout_albedo;
layout (location = 1) out highp uvec2 texout_normal;
layout (location = 2) out highp uint texout_depth;

in lowp vec2 uv;
in highp vec3 var_pos_wrt_cam;
in highp vec3 var_normal;
in highp float is_curtain;
//flat in vec3 vertex_color;
in highp vec3 vertex_color;
in highp float drop_frag;
in highp vec3 debug_overlay_color;

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

highp vec3 normal_by_fragment_position_interpolation() {
    highp vec3 dFdxPos = dFdx(var_pos_wrt_cam);
    highp vec3 dFdyPos = dFdy(var_pos_wrt_cam);
    return normalize(cross(dFdxPos, dFdyPos));
}

lowp const int steepness_bins = 9;
highp const vec4 steepness_color_map[steepness_bins] = vec4[](
    vec4(254.0/255.0, 249.0/255.0, 249.0/255.0, 1.0),
    vec4(51.0/255.0, 249.0/255.0, 49.0/255.0, 1.0),
    vec4(242.0/255.0, 228.0/255.0, 44.0/255.0, 1.0),
    vec4(255.0/255.0, 169.0/255.0, 45.0/255.0, 1.0),
    vec4(255.0/255.0, 48.0/255.0, 45.0/255.0, 1.0),
    vec4(255.0/255.0, 79.0/255.0, 249.0/255.0, 1.0),
    vec4(183.0/255.0, 69.0/255.0, 253.0/255.0, 1.0),
    vec4(135.0/255.0, 44.0/255.0, 253.0/255.0, 1.0),
    vec4(49.0/255.0, 49.0/255.0, 253.0/255.0, 1.0)
);

void main() {
    if (conf.wireframe_mode == 2u) {
        texout_albedo = vec3(1.0, 1.0, 1.0);
        return;
    }
    if (is_curtain > 0.0) {
        if (conf.curtain_settings.x == 2.0) {
            texout_albedo = vec3(1.0, 0.0, 0.0);
            return;
        } else if (conf.curtain_settings.x == 0.0) {
            discard;
        }
    } else {
        if (conf.curtain_settings.x == 3.0) {
            discard;
        }
    }

    // Encode distance for readback
    highp float dist = length(var_pos_wrt_cam);
    texout_depth = depthWSEncode1u32(dist);

    // Encode normal
    highp vec3 normal = vec3(0.0);
    if (conf.normal_mode == 0u) normal = normal_by_fragment_position_interpolation();
    else normal = var_normal;
    texout_normal = octNormalEncode2u16(normal);

    lowp vec3 fragColor = texture(texture_sampler, uv).rgb;
    fragColor = mix(conf.material_color.rgb, fragColor, conf.material_color.a);
    texout_albedo = fragColor;

    /*
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


    if (conf.debug_overlay == 1u) {
        highp float steepness = (1.0 - dot(normal, vec3(0.0,0.0,1.0)));
        highp float alpha_line = 1.0 - min((dist / 20000.0), 1.0);
        lowp int bin_index = int(steepness * float(steepness_bins - 1) + 0.5);
        texout_albedo = mix(texout_albedo.rgb, steepness_color_map[bin_index].rgb, steepness_color_map[bin_index].a * conf.debug_overlay_strength * alpha_line);
    }

    // == HEIGHT LINES ==============
    if (bool(conf.height_lines_enabled)) {
        highp float alpha_line = 1.0 - min((dist / 20000.0), 1.0);
        highp float line_width = (2.0 + dist / 5000.0) * 5.0;
        // Calculate steepness based on fragment normal (this alone gives woobly results)
        highp float steepness = (1.0 - dot(normal, vec3(0.0,0.0,1.0))) / 2.0;
        // Discretize the steepness -> Doesnt work
        //float steepness_discretized = int(steepness * 10.0f) / 10.0f;
        line_width = line_width * max(0.01,steepness);
        if (alpha_line > 0.05)
        {
            highp float alt = var_pos_wrt_cam.z + camera.position.z;
            highp float alt_rest = (alt - float(int(alt / 100.0)) * 100.0) - line_width / 2.0;
            if (alt_rest < line_width) {
                texout_albedo = mix(texout_albedo, vec4(texout_albedo.r - 0.2, texout_albedo.g - 0.2, texout_albedo.b - 0.2, 1.0), alpha_line);
            }
        }
    }
}

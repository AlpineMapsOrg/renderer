/*****************************************************************************
* Alpine Renderer
* Copyright (C) 2022 Adam Celarek
* Copyright (C) 2023 Gerald Kimmersdorfer
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

layout (location = 0) out lowp vec3 texout_albedo;
layout (location = 1) out highp vec4 texout_position;
layout (location = 2) out highp uvec2 texout_normal;
layout (location = 3) out highp uint texout_depth;

in highp vec2 uv;
in highp vec3 var_pos_cws;
in highp vec3 var_normal;
flat in lowp uint is_curtain;
flat in lowp vec3 vertex_color;

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

highp vec3 normal_by_fragment_position_interpolation() {
    highp vec3 dFdxPos = dFdx(var_pos_cws);
    highp vec3 dFdyPos = dFdy(var_pos_cws);
    return normalize(cross(dFdxPos, dFdyPos));
}

void main() {
    // ToDo: Fix the following. They are not correct... (no normal for only curtains?)
    if (conf.wireframe_mode == 2u) {
        texout_albedo = vec3(1.0, 1.0, 1.0);
        return;
    }
    if (is_curtain > 0u) {
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

    // Write Albedo (ortho picture) in gbuffer
    lowp vec3 fragColor = texture(texture_sampler, uv).rgb;
    fragColor = mix(fragColor, conf.material_color.rgb, conf.material_color.a);
    texout_albedo = fragColor;

    // Write Position (and distance) in gbuffer
    highp float dist = length(var_pos_cws);
    texout_position = vec4(var_pos_cws, dist);

    // Write and encode normal in gbuffer
    highp vec3 normal = vec3(0.0);
    if (conf.normal_mode == 0u) normal = normal_by_fragment_position_interpolation();
    else normal = var_normal;
    texout_normal = octNormalEncode2u16(normal);

    // Write and encode distance for readback
    texout_depth = depthWSEncode1u32(dist);

    // HANDLE OVERLAYS (and mix it with the albedo color) THAT CAN JUST BE DONE IN THIS STAGE
    // (because of DATA thats not forwarded)
    // NOTE: Performancewise its generally better to handle overlays in the compose step! (screenspace effect)
    if (conf.overlay_mode > 0u && conf.overlay_mode < 100u) {
        lowp vec3 overlay_color = vec3(0.0);
        switch(conf.overlay_mode) {
            case 1u: overlay_color = normal * 0.5 + 0.5; break;
            default: overlay_color = vertex_color;
        }
        texout_albedo = mix(texout_albedo, overlay_color, conf.overlay_strength);
    }


    // == HEIGHT LINES ============== TODO: Move to compose
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
            highp float alt = var_pos_cws.z + camera.position.z;
            highp float alt_rest = (alt - float(int(alt / 100.0)) * 100.0) - line_width / 2.0;
            if (alt_rest < line_width) {
                texout_albedo = mix(texout_albedo, vec3(texout_albedo.r - 0.2, texout_albedo.g - 0.2, texout_albedo.b - 0.2), alpha_line);
            }
        }
    }
}

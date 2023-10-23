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

#include "camera_config.glsl"
#include "shared_config.glsl"
#include "encoder.glsl"

const lowp uint MAX_SSAO_KERNEL_SIZE = 64u;   // also change in SSAO.h

layout (location = 0) out highp float out_color;

in highp vec2 texcoords;

uniform highp sampler2D texin_depth;
uniform highp usampler2D texin_normal;
uniform highp sampler2D texin_noise;

uniform highp vec3 samples[MAX_SSAO_KERNEL_SIZE];

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

void main()
{
    // Reconstruct position from depth buffer
    highp float depth_cs = texture(texin_depth, texcoords).x;
    if (depth_cs == FARPLANE_DEPTH_VALUE) {
        out_color = conf.ssao_falloff_to_value;
        return;
    }

    highp vec3 pos_cws = depth_cs_to_pos_ws(depth_cs, texcoords);
    highp float dist = length(pos_cws);        // Distance to camera

    // tile noise texture over screen based on screen dimensions divided by noise size
    highp vec2 noiseScale = camera.viewport_size / 4.0;

    // get input for SSAO algorithm
    highp vec3 normal_ws = octNormalDecode2u16(texture(texin_normal, texcoords).xy);
    highp vec3 randomVec = normalize(texture(texin_noise, texcoords * noiseScale).xyz);

    // Depth dependet radius.
    highp float radius = dist / 10.0 + 20.0;// / 10.0 + 50.0; //dist / 10.0 + 10.0;
    highp float bias = radius / 1000.0; //radius / 1000.0; //0.0000001; //radius / 1000.0;

    lowp int kernel = int(conf.ssao_kernel);

    highp float falloff = calculate_falloff(dist, 50000.0, 65000.0);

    highp float occlusion = 0.0;

    if (falloff > 0.01) {
        // create TBN change-of-basis matrix: from tangent-space to camera-world-space
        highp vec3 tangent = normalize(randomVec - normal_ws * dot(randomVec, normal_ws));
        highp vec3 bitangent = cross(normal_ws, tangent);
        highp mat3 TBN = mat3(tangent, bitangent, normal_ws);

        // iterate over the sample kernel and calculate occlusion factor
        for(lowp int i = 0; i < kernel; ++i)
        {
            // NOTE: Sadly a lot of transformations are happening in the following code. The issue
            // is that we should compare in worldspace because the depth buffer is nonlinear.
            // There is definitely a better solution out there :D

            // get sample position in world space
            highp vec3 sample_pos_cws = TBN * samples[i];
            sample_pos_cws = pos_cws + sample_pos_cws * radius;
            highp float sample_gt_dist = length(sample_pos_cws);

            // project sample position (to sample texture)
            highp vec3 sample_pos_ndc = ws_to_ndc(sample_pos_cws);

            // get actual depth and actual distance to camera
            highp float sample_depth_cs = texture(texin_depth, sample_pos_ndc.xy).x;
            highp float sample_dist = length(depth_cs_to_pos_ws(sample_depth_cs, sample_pos_ndc.xy));

            // range check & accumulate
            highp float rangeCheck = 1.0;
            if (bool(conf.ssao_range_check)) {
                rangeCheck = smoothstep(0.0, 1.0, radius / abs(dist - sample_dist));
            }
            occlusion += (sample_gt_dist >= sample_dist + bias ? 1.0 : 0.0) * rangeCheck;
        }
        occlusion = 1.0 - (occlusion / float(kernel));
    }
    occlusion = occlusion * occlusion * occlusion;
    out_color = mix(conf.ssao_falloff_to_value, occlusion, falloff);
}


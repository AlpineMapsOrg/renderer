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

const uint MAX_SSAO_KERNEL_SIZE = 64u;   // also change in SSAO.h

layout (location = 0) out highp float out_color;

in highp vec2 texcoords;

uniform sampler2D texin_position;
uniform sampler2D texin_normal;
uniform sampler2D texin_noise;

uniform vec3 samples[MAX_SSAO_KERNEL_SIZE];

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

void main()
{
    // tile noise texture over screen based on screen dimensions divided by noise size
    vec2 noiseScale = camera.viewport_size / 4.0;

    // get input for SSAO algorithm
    vec3 pos_cws = texture(texin_position, texcoords).xyz;
    vec4 normal_dist = texture(texin_normal, texcoords);
    vec3 normal_ws = normalize(normal_dist.xyz);
    float dist = normal_dist.w;

    vec3 randomVec = normalize(texture(texin_noise, texcoords * noiseScale).xyz);

    // Depth dependet radius.
    float radius = dist / 10.0 + 10.0;
    float bias = radius / 1000.0;
    int kernel = int(conf.ssao_kernel);

    float falloff = calculate_falloff(dist, 50000, 65000);

    float occlusion = 0.0;

    if (falloff > 0.01) {
        // create TBN change-of-basis matrix: from tangent-space to camera-world-space
        vec3 tangent = normalize(randomVec - normal_ws * dot(randomVec, normal_ws));
        vec3 bitangent = cross(normal_ws, tangent);
        mat3 TBN = mat3(tangent, bitangent, normal_ws);

        // iterate over the sample kernel and calculate occlusion factor
        for(int i = 0; i < kernel; ++i)
        {
            // get sample position
            vec3 sample_pos_cws = TBN * samples[i];
            sample_pos_cws = pos_cws + sample_pos_cws * radius;
            float sample_gt_dist = length(sample_pos_cws);

            // project sample position (to sample texture)
            vec3 sample_pos_ndc = ws_to_ndc(sample_pos_cws);

            // get actual distance to camera of fragment (currently saved inside normal texture)
            float sample_dist = texture(texin_normal, sample_pos_ndc.xy).w;

            // range check & accumulate
            float rangeCheck = 1.0;
            if (conf.ssao_range_check) {
                rangeCheck = smoothstep(0.0, 1.0, radius / abs(dist - sample_dist));
            }
            occlusion += (sample_gt_dist >= sample_dist + bias ? 1.0 : 0.0) * rangeCheck;
        }
        occlusion = 1.0 - (occlusion / kernel);

    }
    occlusion = occlusion * occlusion * occlusion;
    out_color = mix(conf.ssao_falloff_to_value, occlusion, falloff);
}


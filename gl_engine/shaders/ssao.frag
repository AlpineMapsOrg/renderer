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

layout (location = 0) out highp float out_color;

in highp vec2 texcoords;

uniform sampler2D texin_position;
uniform sampler2D texin_normal;
uniform sampler2D texin_noise;

uniform vec3 samples[64];

// parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
int kernelSize = 64;
//float radius = 20;
float bias = 0.001;

highp float calculate_falloff(highp float dist, highp float from, highp float to) {
    return clamp(1.0 - (dist - from) / (to - from), 0.0, 1.0);
}

void main()
{
    // tile noise texture over screen based on screen dimensions divided by noise size
    vec2 noiseScale = camera.viewport_size / 4.0;

    // get input for SSAO algorithm
    vec3 pos_wrt_cam = texture(texin_position, texcoords).xyz;
    vec4 normal_dist = texture(texin_normal, texcoords);
    vec3 normal_ws = normalize(normal_dist.xyz);
    float dist = normal_dist.w;

    vec3 pos_vs = vec3(camera.view_matrix * vec4(pos_wrt_cam, 1.0));
    vec3 normal_vs = vec3(camera.view_matrix * vec4(normal_ws, 0.0));
    //vec3 pos_vs = texture(texin_position, texcoords).xyz;
    //vec3 normal_vs = normalize(texture(texin_normal, texcoords).xyz);
    vec3 randomVec = normalize(texture(texin_noise, texcoords * noiseScale).xyz);

    // Depth dependet radius.
    float radius = dist / 10 + 0;

    float falloff = calculate_falloff(dist, 50000, 65000);

    float occlusion = 0.0;

    if (falloff < 0.01) {
        occlusion = 0.0;
    } else {
        // create TBN change-of-basis matrix: from tangent-space to view-space
        vec3 tangent = normalize(randomVec - normal_vs * dot(randomVec, normal_vs));
        vec3 bitangent = cross(normal_vs, tangent);
        mat3 TBN = mat3(tangent, bitangent, normal_vs);
        // iterate over the sample kernel and calculate occlusion factor

        for(int i = 0; i < kernelSize; ++i)
        {
            // get sample position
            vec3 samplePos = TBN * samples[i]; // from tangent to view-space
            samplePos = pos_vs + samplePos * radius;

            // project sample position (to sample texture) (to get position on screen/texture)
            vec4 offset = vec4(samplePos, 1.0);
            offset = camera.proj_matrix * offset; // from view to clip-space
            offset.xyz /= offset.w; // perspective divide
            offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0

            // get sample depth
            float sampleDepth = vec3(camera.view_matrix * vec4(texture(texin_position, offset.xy).xyz, 1.0)).z;
            //float sampleDepth = texture(texin_position, offset.xy).z; // get depth value of kernel sample

            // range check & accumulate
            float rangeCheck = smoothstep(0.0, 1.0, radius / abs(pos_vs.z - sampleDepth));
            occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
        }
        occlusion = 1.0 - (occlusion / kernelSize);
        occlusion = occlusion * occlusion * falloff;
    }

    out_color = occlusion;
    //out_color = falloff;
}



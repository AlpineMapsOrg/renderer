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

layout (location = 0) out highp float out_color;

in highp vec2 texcoords;

uniform sampler2D texin_position;
uniform sampler2D texin_normal;
uniform sampler2D texin_noise;

uniform vec3 samples[64];

// parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
int kernelSize = 64;
float radius = 20;
float bias = 0.1;

// tile noise texture over screen based on screen dimensions divided by noise size
const vec2 noiseScale = vec2(2560.0/4.0, 1377.0/4.0);

uniform mat4 proj_matrix;

void main()
{

    // get input for SSAO algorithm
    vec3 fragPos = texture(texin_position, texcoords).xyz;
    vec3 normal = normalize(texture(texin_normal, texcoords).rgb);
    vec3 randomVec = normalize(texture(texin_noise, texcoords * noiseScale).xyz);

    // create TBN change-of-basis matrix: from tangent-space to view-space
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position
        vec3 samplePos = TBN * samples[i]; // from tangent to view-space
        samplePos = fragPos + samplePos * radius;

        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(samplePos, 1.0);
        offset = proj_matrix * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0

        // get sample depth
        float sampleDepth = texture(texin_position, offset.xy).z; // get depth value of kernel sample

        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / kernelSize);

    out_color = occlusion * occlusion;
    //out_color = normal.x * fragPos.x * randomVec.x * samples[63].x;
}



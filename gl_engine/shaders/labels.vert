/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

// we interpolate between both far labels depending on importance
// -> if importance is 1 -> we will show the label from farther away
const float farLabel0 = 50000.0f;
const float farLabel1 = 500000.0f;
const float nearLabel = 100.0f;

const vec2 offset_mask[4] = vec2[4](vec2(0.0f,0.0f), vec2(0.0f,1.0f), vec2(1.0f,1.0f), vec2(1.0f,0.0f));

const int peak_visibilty_count = 4;
// const float peak_visibilty_marker_far[peak_visibilty_count] = float[](400, 300, 200, 100);
const float peak_visibilty_marker[peak_visibilty_count] = float[](0.06f, 0.04f, 0.02f, 0.0f);
const float peak_visibilty_weight[peak_visibilty_count] = float[](0.4f, 0.2f, 0.2f, 0.4f);

uniform highp mat4 inv_view_rot;
uniform bool label_dist_scaling;

uniform sampler2D texin_depth;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 vtexcoords;
layout (location = 2) in vec3 label_position;
layout (location = 3) in float importance;

out highp vec2 texcoords;
flat out highp float opacity;
flat out highp float distance;

float determineLabelOcclusionVisibilty(float dist)
{
    opacity = 0.0f;
    for(lowp int i = 0; i < peak_visibilty_count; ++i)
    {
        vec3 peakLookup;
        if(dist > 5000.0f)
        {
            // case: peak is far away
            // -> we prefer to look for obfuscation above the peak
            // -> we still want to see where a peak is even if it is obfuscated a little bit
            peakLookup = ws_to_ndc((label_position - camera.position.xyz)) + vec3(0.0f, peak_visibilty_marker[i],0.0f);
        }
        else
        {
            // case: near to the peak
            // -> we prefer actual obfuscations of the peak (if we cant see the peak from the current location we dont want to show the label at all)
            peakLookup = ws_to_ndc((label_position - camera.position.xyz)) - vec3(0.0f, peak_visibilty_marker[peak_visibilty_count-i-1],0.0f);
        }

        float depth = texture(texin_depth, peakLookup.xy).w;

        // check if our distance is nearer than the depth test (we subtract a small number in order to prevent precision errors in the lookup
        // depth == 0 if we hit the skybox -> we therefore test additionally if depth is smaller than a small epsilon
        if(depth <= 0.001f || depth > (dist-200.0f))
        {
            // opacity = 1.0 - clamp(c.w/250000.0, 0.0, 1.0);
            opacity += peak_visibilty_weight[peak_visibilty_count-i-1];
        }
        else if(i == 0)
        {
            // first marker is not visible -> discard it completely
            return 0.0f;
        }

    }

    return opacity;
}

void main() {
    float dist = length(label_position - camera.position.xyz);
    distance = dist;
    // remove distance scaling of labels
    float scale = dist / (camera.viewport_size.y * 0.5f * camera.distance_scaling_factor);

     // we are interpolating the far label according to importance
    float interpolatedFarLabelDistance = mix(farLabel0, farLabel1, importance);

    // apply "soft" distance scaling depending on near/far label values (if option is set as uniform)
    if(label_dist_scaling)
    {
        float dist_scale = 1.0f - ((dist - nearLabel) / (farLabel1 - nearLabel)) * 0.4f;
        scale *= (dist_scale * dist_scale);
    }

    // importance based scaling
    scale *= (importance + 1.5f) / 2.5f;

    // remove rotation from position -> since we want to always face the camera; and apply the scaling
    vec4 rotationless_pos = (inv_view_rot * vec4((pos.xy + pos.zw * offset_mask[gl_VertexID]) * scale, 0.0f, 1.0f));
    rotationless_pos /= rotationless_pos.w;

    // apply camera matrix and position the label depending on world/camera position
    gl_Position = camera.view_proj_matrix * vec4((label_position + vec3(0, 5, 0) - camera.position.xyz) + rotationless_pos.xyz, 1.0f);

    // get opacity from occlusion
    opacity = determineLabelOcclusionVisibilty(dist);

    // we are reducing the opacity depending on distance
    float distance_opacity = 1.0f - clamp(((dist - nearLabel) / (interpolatedFarLabelDistance - nearLabel)), 0.0f, 1.0f);
    opacity *= (distance_opacity*distance_opacity);

    // discard this vertex if we are occluded by the terrain and or distance
    if(opacity <= 0.37f)
    {
        gl_Position = vec4(10.0f,10.0f,10.0f,1.0f);
    }
    // else if(opacity <= 0.57f)
    // {
    //     // fade only between opacity 0.37 to 0.57
    //     opacity = (opacity-0.37f) / 0.2f;
    // }
    else
    {
        opacity = 1.0f;
    }

    // for visibilties sake we have a minimum opacity of 0.5
    // opacity = clamp(0.4 + opacity, 0.3, 1.0);

    // uv coordinates
    texcoords = vtexcoords.xy + vtexcoords.zw * offset_mask[gl_VertexID];
}

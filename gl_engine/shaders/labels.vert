/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Adam Celarek
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

uniform highp mat4 inv_view_rot;
uniform bool label_dist_scaling;

uniform sampler2D texin_depth;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 vtexcoords;
layout (location = 2) in vec3 label_position;
layout (location = 3) in float importance;

out highp vec2 texcoords;
out highp float dist_to_cam;

bool label_visible(highp vec3 relative_to_cam, float dist_to_cam) {
    if (importance < 0.2 && dist_to_cam > 3000.0)
        return false;
    if (importance < 0.4 && dist_to_cam > 20000.0)
        return false;
    if (importance < 0.6 && dist_to_cam > 250000.0)
        return false;
    if (importance < 0.8 && dist_to_cam > 500000.0)
        return false;

    vec3 peakLookup = ws_to_ndc(relative_to_cam) + vec3(0.0f, 0.1f, 0.0f);
    float depth = texture(texin_depth, peakLookup.xy).w;
    if(depth <= 0.001f || depth > (dist_to_cam-200.0f))
    {
        return true;
    }
    return false;
}

void main() {
    highp vec3 relative_to_cam = label_position - camera.position.xyz;
    dist_to_cam = length(relative_to_cam);
    float scale = 2.0f;

    // apply "soft" distance scaling depending on near/far label values (if option is set as uniform)
    if(label_dist_scaling)
    {
        float dist_scale = 1.0f - ((dist_to_cam - nearLabel) / (farLabel1 - nearLabel)) * 0.4f;
        scale *= (dist_scale * dist_scale);
    }

    // importance based scaling
    scale *= (importance + 1.5f) / 2.5f;

    if (label_visible(relative_to_cam, dist_to_cam)) {
        gl_Position = camera.view_proj_matrix * vec4(relative_to_cam + vec3(0.0, 0.0, 5.0), 1.0f);
        gl_Position /= gl_Position.w;
        gl_Position += vec4((pos.xy + pos.zw * offset_mask[gl_VertexID & 3]) * vec2(0.5f / camera.viewport_size.x, 0.5f / camera.viewport_size.y), 0.0f, 0.0f) * scale;
        // uv coordinates
        texcoords = vtexcoords.xy + vtexcoords.zw * offset_mask[gl_VertexID & 3];
    }
    else {
        gl_Position = vec4(10.0f, 10.0f, 10.0f, 1.0f);
    }

}

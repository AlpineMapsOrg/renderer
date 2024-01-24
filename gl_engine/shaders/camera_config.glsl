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

layout (std140) uniform camera_config {
    highp vec4 position;
    highp mat4 view_matrix;
    highp mat4 proj_matrix;
    highp mat4 view_proj_matrix;
    highp mat4 inv_view_proj_matrix;
    highp mat4 inv_view_matrix;
    highp mat4 inv_proj_matrix;
    highp vec2 viewport_size;
    highp float distance_scaling_factor;
    highp float buffer2;
} camera;

// Converts the given world coordinates (relative to camera) to the
// normalised device coordinates (range [0,1])
highp vec3 ws_to_ndc(highp vec3 pos_ws) {
    highp vec4 tmp = camera.view_proj_matrix * vec4(pos_ws, 1.0); // from ws to clip-space
    tmp.xyz /= tmp.w; // perspective divide
    return tmp.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
}

highp vec3 depth_cs_to_pos_ws(highp float depth, highp vec2 tex_coords) {
    highp vec4 clip_space_position = vec4(tex_coords * 2.0 - vec2(1.0), 2.0 * depth - 1.0, 1.0);
    highp vec4 position = camera.inv_view_proj_matrix * clip_space_position; // Use this for world space
    return(position.xyz / position.w);
}

highp vec3 depth_cs_to_pos_vs(highp float depth, highp vec2 tex_coords) {
    highp vec4 clip_space_position = vec4(tex_coords * 2.0 - vec2(1.0), 2.0 * depth - 1.0, 1.0);
    highp vec4 position = camera.inv_proj_matrix * clip_space_position; // Use this for view space
    return(position.xyz / position.w);
}

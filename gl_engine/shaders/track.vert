/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2024 Jakob Maier
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

#include "overlay_steepness.glsl"

layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_direction;
layout(location = 2) in highp vec3 a_offset;

uniform highp mat4 view;
uniform highp mat4 proj;
uniform highp vec3 camera_position;
uniform highp float width;
uniform highp sampler2D texin_track;

flat out highp int vertex_id;

void main() {

  highp mat4 matrix = proj * view;

  vertex_id = int(a_offset.y);

  highp vec3 x0 = a_position - camera_position;

  // main axis of the rounded cone
  highp vec3 main_axis = a_direction;

  highp vec3 view_direction = camera_position - a_position;

  // orthogonal to main axis
  highp vec3 u_hat = normalize(cross(main_axis, view_direction));

  highp vec3 v0 = normalize(cross(u_hat, view_direction));

  highp float t0 = sqrt(dot(view_direction, view_direction) - (width * width));

  highp vec3 p0 = x0 + (a_offset.x * v0 * width);

  highp vec3 position = p0 + u_hat * a_offset.z * width;

  gl_Position = matrix * vec4(position, 1);
}

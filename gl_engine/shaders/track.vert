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
//layout(location = 3) in highp vec3 a_metadata; // data like speed, vertical speed, etc...

uniform highp mat4 view;
uniform highp mat4 proj;
uniform highp vec3 camera_position;
uniform highp float width;
uniform highp sampler2D texin_track;

flat out highp int vertex_id;
flat out highp float radius;

#define METHOD 2

void main() {

  highp mat4 matrix = proj * view;

  vertex_id = int(a_offset.y);

  radius = width;

  int shading_method = 0;

  highp vec3 e = camera_position;
  highp vec3 x0 = a_position - camera_position;

  // main axis of the rounded cone
  highp vec3 d = a_direction;

  highp vec3 d0 = camera_position - a_position;

  // orthogonal to main axis
  highp vec3 u_hat = normalize(cross(d, d0));

  highp vec3 v0 = normalize(cross(u_hat, d0));


  highp float r0 = radius; // cone end cap radius

  highp float t0 = sqrt(dot(d0, d0) - (r0 * r0));

  // TODO
  //float r0_prime = length(d0) * (r0 / t0);
  highp float r0_prime = r0;


  // TODO:
  highp float r0_double_prime = r0;

  highp vec3 p0 = x0 + (a_offset.x * v0 * r0_prime);

  highp vec3 position = p0 + u_hat * a_offset.z * r0_double_prime;

  gl_Position = matrix * vec4(position, 1);
}

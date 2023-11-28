/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

layout (location = 0) in vec3 pos;
uniform highp vec3 label_position;
uniform highp mat4 inv_view_rot;
uniform highp mat4 scale_matrix;
void main() {
    // TODO here view_proj_matrix is local.... -> we have to translate by camera
//    mat3 invViewRot = inverse(mat3(camera.view_matrix));
//    vec4 rotationless_pos =  (inv_proj_scale * inv_view_rot * vec4(pos,1.0)); // remove rotation from position -> since we want to always face the camera
//    vec4 rotationless_pos =  (scale_matrix * inv_view_rot * vec4(pos,1.0)); // remove rotation from position -> since we want to always face the camera
//    vec4 rotationless_pos =  (scale_matrix * vec4(pos,1.0)); // remove rotation from position -> since we want to always face the camera
//    rotationless_pos /= rotationless_pos.w;
    vec4 rotationless_pos =  (inv_view_rot * scale_matrix * vec4(pos,1.0)); // remove rotation from position -> since we want to always face the camera
    rotationless_pos /= rotationless_pos.w;

    vec4 p = camera.view_proj_matrix * (vec4(label_position + rotationless_pos.xyz - camera.position.xyz, 1.0));
    p /= p.w;
    gl_Position = p;


}

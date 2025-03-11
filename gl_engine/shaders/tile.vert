/*****************************************************************************
* Alpine Renderer
* Copyright (C) 2022 Adam Celarek
* Copyright (C) 2023 Gerald Kimmersdorfer
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

#include "shared_config.glsl"
#include "hashing.glsl"
#include "camera_config.glsl"
#include "tile.glsl"

out highp vec2 var_uv;
out highp vec3 var_pos_cws;
out highp vec3 var_normal;
flat out highp uvec3 var_tile_id;
#if CURTAIN_DEBUG_MODE > 0
out lowp float is_curtain;
#endif
flat out lowp vec3 vertex_color;
flat out highp uint instance_id;


void main() {
    compute_vertex(var_pos_cws, var_uv, var_tile_id, conf.normal_mode == 1u, var_normal);

    gl_Position = camera.view_proj_matrix * vec4(var_pos_cws, 1);
    instance_id = uint(gl_InstanceID);

    vertex_color = vec3(0.0);
    switch(conf.overlay_mode) {
        case 2u: vertex_color = color_from_id_hash(uint(var_tile_id.x ^ var_tile_id.y)); break;
        case 3u: vertex_color = color_from_id_hash(uint(var_tile_id.z)); break;
        case 4u: vertex_color = color_from_id_hash(uint(gl_VertexID)); break;
        case 5u: vertex_color = vec3(var_pos_cws + camera.position.xyz).zzz / 4096.0; break;
    }
}

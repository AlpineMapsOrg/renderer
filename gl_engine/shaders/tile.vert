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
#include "tile_id.glsl"

out highp vec2 var_uv;
out highp vec3 var_pos_cws;
out highp vec3 var_normal;
flat out highp uvec3 var_tile_id;
#if CURTAIN_DEBUG_MODE > 0
out lowp float is_curtain;
#endif
flat out lowp vec3 vertex_color;


void main() {
    float n_quads_per_direction;
    float quad_width;
    float quad_height;
    float altitude_correction_factor;
    var_pos_cws = camera_world_space_position(var_uv, n_quads_per_direction, quad_width, quad_height, altitude_correction_factor);

    if (conf.normal_mode == 1u) {
        var_normal = normal_by_finite_difference_method(var_uv, n_quads_per_direction, quad_width, quad_height, altitude_correction_factor);
    }

    var_tile_id = unpack_tile_id(packed_tile_id);
    gl_Position = camera.view_proj_matrix * vec4(var_pos_cws, 1);

    vertex_color = vec3(0.0);
    switch(conf.overlay_mode) {
        case 2u: vertex_color = color_from_id_hash(uint(packed_tile_id.x ^ packed_tile_id.y)); break;
        case 3u: vertex_color = color_from_id_hash(uint(var_tile_id.z)); break;
        case 4u: vertex_color = color_from_id_hash(uint(gl_VertexID)); break;
        case 5u: vertex_color = vec3(texture(height_sampler, vec3(var_uv, height_texture_layer)).rrr) / 65535.0; break;
    }
}

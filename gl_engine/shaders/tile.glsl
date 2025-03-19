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

#include "tile_id.glsl"
#line 22

layout(location = 0) in highp vec4 instance_bounds;
layout(location = 1) in highp uvec2 instance_tile_id_packed;
layout(location = 2) in mediump uint dtm_array_index;
layout(location = 3) in lowp uint dtm_zoom;

uniform highp int n_edge_vertices;
uniform mediump usampler2DArray height_tex_sampler;

highp float y_to_lat(highp float y) {
    const highp float pi = 3.1415926535897932384626433;
    const highp float cOriginShift = 20037508.342789244;

    highp float mercN = y * pi / cOriginShift;
    highp float latRad = 2.f * (atan(exp(mercN)) - (pi / 4.0));
    return latRad;
}


void compute_vertex(out vec3 position, out vec2 uv, out uvec3 tile_id, bool compute_normal, out vec3 normal) {
    tile_id = unpack_tile_id(instance_tile_id_packed);

    highp uvec3 dtm_tile_id = tile_id;
    {
        // uint dtm_zoom = texelFetch(instance_2_zoom_sampler, ivec2(uint(gl_InstanceID), 0), 0).x;
        decrease_zoom_level_until(dtm_tile_id, dtm_zoom);
    }
    highp int n_quads_per_direction_int = (n_edge_vertices - 1) >> (tile_id.z - dtm_tile_id.z);
    highp float n_quads_per_direction = float(n_quads_per_direction_int);
    highp float quad_size = (instance_bounds.z - instance_bounds.x) / n_quads_per_direction;

    highp int row = gl_VertexID / n_edge_vertices;
    highp int col = gl_VertexID - (row * n_edge_vertices);
    highp int curtain_vertex_id = gl_VertexID - n_edge_vertices * n_edge_vertices;
    if (curtain_vertex_id >= 0) {
        if (curtain_vertex_id < n_edge_vertices) {
            row = (n_edge_vertices - 1) - curtain_vertex_id;
            col = (n_edge_vertices - 1);
        }
        else if (curtain_vertex_id >= n_edge_vertices && curtain_vertex_id < 2 * n_edge_vertices - 1) {
            row = 0;
            col = (n_edge_vertices - 1) - (curtain_vertex_id - n_edge_vertices) - 1;
        }
        else if (curtain_vertex_id >= 2 * n_edge_vertices - 1 && curtain_vertex_id < 3 * n_edge_vertices - 2) {
            row = curtain_vertex_id - 2 * n_edge_vertices + 2;
            col = 0;
        }
        else {
            row = (n_edge_vertices - 1);
            col = curtain_vertex_id - 3 * n_edge_vertices + 3;
        }
    }
    if (row > n_quads_per_direction_int) {
        row = n_quads_per_direction_int;
        curtain_vertex_id = 1;
    }
    if (col > n_quads_per_direction_int) {
        col = n_quads_per_direction_int;
        curtain_vertex_id = 1;
    }

    position.y = float(n_quads_per_direction_int - row) * float(quad_size) + instance_bounds.y;
    position.x = float(col) * quad_size + instance_bounds.x;
    uv = vec2(float(col) / n_quads_per_direction, float(row) / n_quads_per_direction);

    highp vec2 dtm_uv = uv;
    dtm_tile_id = tile_id;
    decrease_zoom_level_until(dtm_tile_id, dtm_uv, dtm_zoom);
    // highp float dtm_texture_layer_f = float(texelFetch(instance_2_array_index_sampler, ivec2(uint(gl_InstanceID), 0), 0).x);
    highp float dtm_texture_layer_f = float(dtm_array_index);
    float altitude_tex = float(texture(height_tex_sampler, vec3(dtm_uv, dtm_texture_layer_f)).r);

    // Note: for higher zoom levels it would be enough to calculate the altitude_correction_factor on cpu
    // for lower zoom levels we could bake it into the texture.
    // there was no measurable difference despite a cos and a atan, so leaving as is for now.
    highp float world_space_y = position.y + camera.position.y;
    highp float altitude_correction_factor = 0.125 / cos(y_to_lat(world_space_y)); // https://github.com/AlpineMapsOrg/renderer/issues/5
    float adjusted_altitude = altitude_tex * altitude_correction_factor;
    position.z = adjusted_altitude - camera.position.z;

    if (curtain_vertex_id >= 0) {
        float curtain_height = CURTAIN_REFERENCE_HEIGHT;
#if CURTAIN_HEIGHT_MODE == 1
        float dist_factor = clamp(length(position) / 100000.0, 0.2, 1.0);
        curtain_height *= dist_factor;
#endif
#if CURTAIN_HEIGHT_MODE == 2
        float zoom_factor = 1.0 - max(0.1, float(tile_id.z) / 25.f);
        curtain_height *= zoom_factor;
#endif
        position.z = position.z - curtain_height;
    }

    if (compute_normal) {
        // from here: https://stackoverflow.com/questions/6656358/calculating-normals-in-a-triangle-mesh/21660173#21660173
        vec2 offset = vec2(1.0, 0.0) / float(n_edge_vertices - 1);

        highp float hL = float(texture(height_tex_sampler, vec3(dtm_uv - offset.xy, dtm_texture_layer_f)).r);
        hL *= altitude_correction_factor;
        highp float hR = float(texture(height_tex_sampler, vec3(dtm_uv + offset.xy, dtm_texture_layer_f)).r);
        hR *= altitude_correction_factor;
        highp float hD = float(texture(height_tex_sampler, vec3(dtm_uv + offset.yx, dtm_texture_layer_f)).r);
        hD *= altitude_correction_factor;
        highp float hU = float(texture(height_tex_sampler, vec3(dtm_uv - offset.yx, dtm_texture_layer_f)).r);
        hU *= altitude_correction_factor;

        highp float threshold = 0.5 * offset.x;
        highp float quad_height = quad_size;
        highp float quad_width = quad_size;
        if (dtm_uv.x < threshold || dtm_uv.x > 1.0 - threshold )
            quad_width = quad_size / 2.0;                // half on the edge of a tile_id_packed, as the height texture is clamped

        if (dtm_uv.y < threshold || dtm_uv.y > 1.0 - threshold )
            quad_height = quad_size / 2.0;

        normal = normalize(vec3((hL - hR)/quad_width, (hD - hU)/quad_height, 2.));
    }
}

void compute_vertex(out vec3 position) {
    highp vec2 uv;
    highp uvec3 tile_id;
    vec3 normal;
    compute_vertex(position, uv, tile_id, false, normal);
}

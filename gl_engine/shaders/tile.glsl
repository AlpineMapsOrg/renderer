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
 
layout(location = 0) in highp vec4 bounds;
layout(location = 1) in highp int height_texture_layer;
layout(location = 2) in highp uvec2 packed_tile_id;

uniform highp int n_edge_vertices;
uniform mediump usampler2DArray height_sampler;

highp float y_to_lat(highp float y) {
    const highp float pi = 3.1415926535897932384626433;
    const highp float cOriginShift = 20037508.342789244;

    highp float mercN = y * pi / cOriginShift;
    highp float latRad = 2.f * (atan(exp(mercN)) - (pi / 4.0));
    return latRad;
}

highp vec3 camera_world_space_position(out vec2 uv, out float n_quads_per_direction, out float quad_width, out float quad_height, out float altitude_correction_factor) {
    highp int n_quads_per_direction_int = n_edge_vertices - 1;
    n_quads_per_direction = float(n_quads_per_direction_int);
    quad_width = (bounds.z - bounds.x) / n_quads_per_direction;
    quad_height = (bounds.w - bounds.y) / n_quads_per_direction;

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
    // Note: for higher zoom levels it would be enough to calculate the altitude_correction_factor on cpu
    // for lower zoom levels we could bake it into the texture.
    // but there was no measurable difference despite a cos and a atan, so leaving as is for now.
    highp float var_pos_cws_y = float(n_quads_per_direction_int - row) * float(quad_width) + bounds.y;
    highp float pos_y = var_pos_cws_y + camera.position.y;
    altitude_correction_factor = 0.125 / cos(y_to_lat(pos_y)); // https://github.com/AlpineMapsOrg/renderer/issues/5

    uv = vec2(float(col) / n_quads_per_direction, float(row) / n_quads_per_direction);
    float altitude_tex = float(texelFetch(height_sampler, ivec3(col, row, height_texture_layer), 0).r);
    float adjusted_altitude = altitude_tex * altitude_correction_factor;

    highp vec3 var_pos_cws = vec3(float(col) * quad_width + bounds.x, var_pos_cws_y, adjusted_altitude - camera.position.z);

    if (curtain_vertex_id >= 0) {
        float curtain_height = CURTAIN_REFERENCE_HEIGHT;
#if CURTAIN_HEIGHT_MODE == 1
        float dist_factor = clamp(length(var_pos_cws) / 100000.0, 0.2, 1.0);
        curtain_height *= dist_factor;
#endif
        var_pos_cws.z = var_pos_cws.z - curtain_height;
    }

    return var_pos_cws;
}

highp vec3 camera_world_space_position() {
    vec2 uv;
    float n_quads_per_direction;
    float quad_width;
    float quad_height;
    float altitude_correction_factor;
    return camera_world_space_position(uv, n_quads_per_direction, quad_width, quad_height, altitude_correction_factor);
}

highp vec3 normal_by_finite_difference_method(vec2 uv, float edge_vertices_count, float quad_width, float quad_height, float altitude_correction_factor) {
    // from here: https://stackoverflow.com/questions/6656358/calculating-normals-in-a-triangle-mesh/21660173#21660173
    vec2 offset = vec2(1.0, 0.0) / (edge_vertices_count);
    float height = quad_width + quad_height;
    highp float hL = float(texture(height_sampler, vec3(uv - offset.xy, height_texture_layer)).r);
    hL *= altitude_correction_factor;
    highp float hR = float(texture(height_sampler, vec3(uv + offset.xy, height_texture_layer)).r);
    hR *= altitude_correction_factor;
    highp float hD = float(texture(height_sampler, vec3(uv + offset.yx, height_texture_layer)).r);
    hD *= altitude_correction_factor;
    highp float hU = float(texture(height_sampler, vec3(uv - offset.yx, height_texture_layer)).r);
    hU *= altitude_correction_factor;

    return normalize(vec3(hL - hR, hD - hU, height));
}

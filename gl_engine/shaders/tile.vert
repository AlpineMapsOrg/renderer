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

layout(location = 0) in highp float altitude;

uniform highp vec4 bounds[32];
uniform highp int n_edge_vertices;
uniform highp int tileset_id;
uniform highp int tileset_zoomlevel;
uniform highp sampler2D height_sampler;

out highp vec2 uv;
out highp vec3 var_pos_cws;
out highp vec3 var_normal;
#if CURTAIN_DEBUG_MODE > 0
out lowp float is_curtain;
#endif
flat out lowp vec3 vertex_color;

highp float y_to_lat(highp float y) {
    const highp float pi = 3.1415926535897932384626433;
    const highp float cOriginShift = 20037508.342789244;

    highp float mercN = y * pi / cOriginShift;
    highp float latRad = 2.f * (atan(exp(mercN)) - (pi / 4.0));
    return latRad;
}

highp float altitude_from_color(highp vec4 color) {
    return (color.r + color.g / 255.0);
}

highp vec3 normal_by_finite_difference_method(vec2 uv, float edge_vertices_count, float tile_width, float tile_height, float altitude_correction_factor) {
    // from here: https://stackoverflow.com/questions/6656358/calculating-normals-in-a-triangle-mesh/21660173#21660173
    vec2 offset = vec2(1.0, 0.0) / (edge_vertices_count);
    float height = tile_width + tile_height;
    float hL = altitude_from_color(texture(height_sampler, uv - offset.xy)) * altitude_correction_factor;
    float hR = altitude_from_color(texture(height_sampler, uv + offset.xy)) * altitude_correction_factor;
    float hD = altitude_from_color(texture(height_sampler, uv + offset.yx)) * altitude_correction_factor;
    float hU = altitude_from_color(texture(height_sampler, uv - offset.yx)) * altitude_correction_factor;

    return normalize(vec3(hL - hR, hD - hU, height));
}

void main() {
    int geometry_id = 0;
    int edge_vertices_count_int = n_edge_vertices - 1;
    float edge_vertices_count_float = float(edge_vertices_count_int);
    // Note: The following is actually not the tile_width but the primitive/cell width/height
    float tile_width = (bounds[geometry_id].z - bounds[geometry_id].x) / edge_vertices_count_float;
    float tile_height = (bounds[geometry_id].w - bounds[geometry_id].y) / edge_vertices_count_float;
    int row = gl_VertexID / n_edge_vertices;
    int col = gl_VertexID - (row * n_edge_vertices);
    int curtain_vertex_id = gl_VertexID - n_edge_vertices * n_edge_vertices;
#if CURTAIN_DEBUG_MODE > 0
    is_curtain = 0.0;
    if (curtain_vertex_id >= 0) is_curtain = 1.0;
#endif
    if (curtain_vertex_id >= 0) {
        // curtains
        if (curtain_vertex_id < n_edge_vertices) {
            // eastern
            row = (n_edge_vertices - 1) - curtain_vertex_id;
            col = (n_edge_vertices - 1);
        }
        else if (curtain_vertex_id >= n_edge_vertices && curtain_vertex_id < 2 * n_edge_vertices - 1) {
            // northern
            row = 0;
            col = (n_edge_vertices - 1) - (curtain_vertex_id - n_edge_vertices) - 1;
        }
        else if (curtain_vertex_id >= 2 * n_edge_vertices - 1 && curtain_vertex_id < 3 * n_edge_vertices - 2) {
            // western
            row = curtain_vertex_id - 2 * n_edge_vertices + 2;
            col = 0;
        }
        else {
            // southern
            row = (n_edge_vertices - 1);
            col = curtain_vertex_id - 3 * n_edge_vertices + 3;
        }
    }
    // Note: May be enough to calculate altitude_correction_factor per tile on CPU:
    float var_pos_cws_y = float(edge_vertices_count_int - row) * float(tile_width) + bounds[geometry_id].y;
    float pos_y = var_pos_cws_y + camera.position.y;
    float altitude_correction_factor = 65536.0 * 0.125 / cos(y_to_lat(pos_y)); // https://github.com/AlpineMapsOrg/renderer/issues/5

    uv = vec2(float(col) / edge_vertices_count_float, float(row) / edge_vertices_count_float);

    float adjusted_altitude = altitude * altitude_correction_factor;

    var_pos_cws = vec3(float(col) * tile_width + bounds[geometry_id].x,
                       var_pos_cws_y,
                       adjusted_altitude - camera.position.z);

    if (curtain_vertex_id >= 0) {
        float curtain_height = CURTAIN_REFERENCE_HEIGHT;
#if CURTAIN_HEIGHT_MODE == 1
        // NOTE: This is definitely subject for improvement! (Perfect would be a function of the neighbouring zoomlevels!)
        float dist_factor = clamp(length(var_pos_cws) / 100000.0, 0.2, 1.0);
        curtain_height *= dist_factor;
#endif
        var_pos_cws.z = var_pos_cws.z - curtain_height;
    }


    if (conf.normal_mode == 1u) {
        var_normal = normal_by_finite_difference_method(uv, edge_vertices_count_float, tile_width, tile_height, altitude_correction_factor);
    }

    gl_Position = camera.view_proj_matrix * vec4(var_pos_cws, 1);

    vertex_color = vec3(0.0);
    switch(conf.overlay_mode) {
        case 2u: vertex_color = color_from_id_hash(uint(tileset_id)); break;
        case 3u: vertex_color = color_from_id_hash(uint(tileset_zoomlevel)); break;
        case 4u: vertex_color = color_from_id_hash(uint(gl_VertexID)); break;
        case 5u: vertex_color = texture(height_sampler, uv).rgb; break;
    }
}

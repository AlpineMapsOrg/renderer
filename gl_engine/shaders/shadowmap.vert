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

#include "shared_config.glsl"
#include "camera_config.glsl"
#include "shadow_config.glsl"

layout(location = 0) in highp float altitude;

uniform highp vec4 bounds[32];

uniform int n_edge_vertices;
uniform sampler2D height_sampler;

//---------
out lowp vec2 uv;
//---------
out highp vec3 var_pos_wrt_cam;

uniform int current_layer;

float y_to_lat(float y) {
    const float pi = 3.1415926535897932384626433;
    const float cOriginShift = 20037508.342789244;

    float mercN = y * pi / cOriginShift;
    float latRad = 2.f * (atan(exp(mercN)) - (pi / 4.0));
    return latRad;
}

void main() {
    int geometry_id = 0;
    float edge_vertices_count = float(n_edge_vertices - 1);
    // Note: The following is actually not the tile_width but the primitive/cell width/height
    float tile_width = (bounds[geometry_id].z - bounds[geometry_id].x) / edge_vertices_count;
    float tile_height = (bounds[geometry_id].w - bounds[geometry_id].y) / edge_vertices_count;

    int row = gl_VertexID / n_edge_vertices;
    int col = gl_VertexID - (row * n_edge_vertices);
    int curtain_vertex_id = gl_VertexID - n_edge_vertices * n_edge_vertices;
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
    float var_pos_wrt_cam_y = (edge_vertices_count - row) * tile_width + bounds[geometry_id].y;
    float pos_y = var_pos_wrt_cam_y + camera.position.y;
    float altitude_correction_factor = 65536.0 * 0.125 / cos(y_to_lat(pos_y)); // https://github.com/AlpineMapsOrg/renderer/issues/5

    //---------
    uv = vec2(col / edge_vertices_count, row / edge_vertices_count);
    //---------

    float adjusted_altitude = altitude * altitude_correction_factor;

    var_pos_wrt_cam = vec3(float(col) * tile_width + bounds[geometry_id].x,
                       var_pos_wrt_cam_y,
                       adjusted_altitude - camera.position.z);

    if (curtain_vertex_id >= 0) {
        float curtain_height = conf.curtain_settings.z;
        if (conf.curtain_settings.y == 1.0) {
            // NOTE: This is definitely subject for improvement!
            float dist_factor = clamp(length(var_pos_wrt_cam) / 100000.0, 0.2, 1.0);
            curtain_height *= dist_factor;
        }
        var_pos_wrt_cam.z = var_pos_wrt_cam.z - curtain_height;
    }


    gl_Position = shadow.light_space_view_proj_matrix[current_layer] * vec4(var_pos_wrt_cam, 1);
}

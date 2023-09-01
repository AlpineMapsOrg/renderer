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

layout(location = 0) in highp float altitude;

uniform highp mat4 matrix;
uniform highp vec3 camera_position;
uniform highp vec4 bounds[32];
uniform int n_edge_vertices;
uniform int tileset_id;
uniform int tileset_zoomlevel;
uniform sampler2D height_sampler;

out lowp vec2 uv;
out highp vec3 pos_wrt_cam;
out highp vec3 var_normal;
out float is_curtain;
//flat out vec3 vertex_color;
out vec3 vertex_color;

out vec3 debug_overlay_color;

float y_to_lat(float y) {
    const float pi = 3.1415926535897932384626433;
    const float cOriginShift = 20037508.342789244;

    float mercN = y * pi / cOriginShift;
    float latRad = 2.f * (atan(exp(mercN)) - (pi / 4.0));
    return latRad;
}

uint compute_hash(uint a)
{
   uint b = (a+2127912214u) + (a<<12u);
   b = (b^3345072700u) ^ (b>>19u);
   b = (b+374761393u) + (b<<5u);
   b = (b+3551683692u) ^ (b<<9u);
   b = (b+4251993797u) + (b<<3u);
   b = (b^3042660105u) ^ (b>>16u);
   return b;
}

vec3 color_from_id_hash(uint a) {
    uint hash = compute_hash(a);
    return vec3(float(hash & 255u), float((hash >> 8u) & 255u), float((hash >> 16u) & 255u)) / 255.0;
}


float altitude_from_color(vec4 color) {
    return (color.r + color.g / 255.0);
}

vec3 normal_by_finite_difference_method(vec2 uv, float edge_vertices_count, float tile_width, float tile_height, float altitude_correction_factor) {
    // from here: https://stackoverflow.com/questions/6656358/calculating-normals-in-a-triangle-mesh/21660173#21660173
    vec2 offset = vec2(1.0, 0.0) / (edge_vertices_count);
    // NOTE: The following code ensures that on the edges of the tile we take the normals from the vertex thats inset by one vertex

/*
    if (uv.x - offset.x < 0.0) uv = uv + offset; // left edge case
    if (uv.y - offset.x < 0.0) uv = uv + offset.yx; // top edge case
    if (uv.x + offset.x > 1.0) uv = uv - offset; // right edge case
    if (uv.y + offset.x > 1.0) uv = uv - offset.yx; // bottom edge case*/



    //if (uv.y == 0.0) debug_overlay_color = texture(height_sampler, uv).rgb;
    float height = tile_width + tile_height;
    float hL = altitude_from_color(texture(height_sampler, uv - offset.xy)) * altitude_correction_factor;
    float hR = altitude_from_color(texture(height_sampler, uv + offset.xy)) * altitude_correction_factor;
    float hD = altitude_from_color(texture(height_sampler, uv + offset.yx)) * altitude_correction_factor;
    float hU = altitude_from_color(texture(height_sampler, uv - offset.yx)) * altitude_correction_factor;

    return normalize(vec3(hL - hR, hD - hU, height));
}

void main() {
    int geometry_id = 0;
    debug_overlay_color = vec3(0.0);
    float edge_vertices_count = float(n_edge_vertices - 1);
    // Note: The following is actually not the tile_width but the primitive/cell width/height
    float tile_width = (bounds[geometry_id].z - bounds[geometry_id].x) / edge_vertices_count;
    float tile_height = (bounds[geometry_id].w - bounds[geometry_id].y) / edge_vertices_count;
    int row = gl_VertexID / n_edge_vertices;
    int col = gl_VertexID - (row * n_edge_vertices);
    int curtain_vertex_id = gl_VertexID - n_edge_vertices * n_edge_vertices;
    is_curtain = 0;
    if (curtain_vertex_id >= 0) {
        // curtains
        is_curtain = 1;
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
    float pos_wrt_cam_y = (edge_vertices_count - row) * tile_width + bounds[geometry_id].y;
    float pos_y = pos_wrt_cam_y + camera_position.y;
    float altitude_correction_factor = 65536.0 * 0.125 / cos(y_to_lat(pos_y)); // https://github.com/AlpineMapsOrg/renderer/issues/5

    uv = vec2(col / edge_vertices_count, row / edge_vertices_count);

    float adjusted_altitude = altitude * altitude_correction_factor;

    pos_wrt_cam = vec3(float(col) * tile_width + bounds[geometry_id].x,
                       pos_wrt_cam_y,
                       adjusted_altitude - camera_position.z);

    if (curtain_vertex_id >= 0) {
        float curtain_height = conf.curtain_settings.z;
        if (conf.curtain_settings.y == 1.0) {
            // NOTE: This is definitely subject for improvement!
            float dist_factor = clamp(length(pos_wrt_cam) / 100000.0, 0.2, 1.0);
            curtain_height *= dist_factor;
        }
        pos_wrt_cam.z = pos_wrt_cam.z - curtain_height;
    }


    if (conf.normal_mode == 1u) {
        var_normal = normal_by_finite_difference_method(uv, edge_vertices_count, tile_width, tile_height, altitude_correction_factor);
    }

    //if (uv.x == 1.0) debug_overlay_color = vec3(1.0, 0.0, 0.0);

    gl_Position = matrix * vec4(pos_wrt_cam, 1);

    if (conf.debug_overlay == 3u) vertex_color = color_from_id_hash(uint(tileset_id));
    else if (conf.debug_overlay == 4u) vertex_color = color_from_id_hash(uint(tileset_zoomlevel));
    else if (conf.debug_overlay == 5u) vertex_color = color_from_id_hash(uint(gl_VertexID));
    else if (conf.debug_overlay == 6u) vertex_color = texture(height_sampler, uv).rgb;
}

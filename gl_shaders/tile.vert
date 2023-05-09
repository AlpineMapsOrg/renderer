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

layout(location = 0) in highp float altitude;
  uniform highp mat4 matrix;
  uniform highp vec3 camera_position;
  uniform highp vec4 bounds[32];
  uniform int n_edge_vertices;
  out lowp vec2 uv;
  out highp vec3 pos_wrt_cam;

  float y_to_lat(float y) {
      const float pi = 3.1415926535897932384626433;
      const float cOriginShift = 20037508.342789244;

      float mercN = y * pi / cOriginShift;
      float latRad = 2.f * (atan(exp(mercN)) - (pi / 4.0));
      return latRad;
  }

  void main() {
      int geometry_id = 0;
      float tile_width = (bounds[geometry_id].z - bounds[geometry_id].x) / float(n_edge_vertices - 1);
      float tile_height = (bounds[geometry_id].w - bounds[geometry_id].y) / float(n_edge_vertices - 1);

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
        else if (curtain_vertex_id >= n_edge_vertices && curtain_vertex_id < 2 * n_edge_vertices) {
            // northern
            row = 0;
            col = (n_edge_vertices - 1) - (curtain_vertex_id - n_edge_vertices);
        }
        else if (curtain_vertex_id >= 2 * n_edge_vertices && curtain_vertex_id < 3 * n_edge_vertices) {
            // western
            row = curtain_vertex_id - 2 * n_edge_vertices;
            col = 0;
        }
        else if (curtain_vertex_id >= 3 * n_edge_vertices && curtain_vertex_id < 4 * n_edge_vertices) {
            // western
            row = (n_edge_vertices - 1);
            col = curtain_vertex_id - 3 * n_edge_vertices;
        }
        else {
            row = (n_edge_vertices - 1) / 2;
            col = (n_edge_vertices - 1) / 2;
        }
    }
    float pos_wrt_cam_y = float(n_edge_vertices - row - 1) * tile_width + bounds[geometry_id].y;
    float pos_y = pos_wrt_cam_y + camera_position.y;
    float adjusted_altitude = altitude * 65536.0 * 0.125 / cos(y_to_lat(pos_y));

    if (curtain_vertex_id >= 0) {
        adjusted_altitude = adjusted_altitude - 500.0;
    }


    pos_wrt_cam = vec3(float(col) * tile_width + bounds[geometry_id].x,
                       pos_wrt_cam_y,
                       adjusted_altitude - camera_position.z);
    uv = vec2(float(col) / float(n_edge_vertices - 1), float(row) / float(n_edge_vertices - 1));
    gl_Position = matrix * vec4(pos_wrt_cam, 1);
  }

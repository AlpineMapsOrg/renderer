layout(location = 0) in highp float height;

uniform highp mat4 matrix;
uniform highp mat4 shadow_matrix;
uniform highp vec3 camera_position;
uniform highp vec3 sun_position;
uniform highp vec4 bounds[32];
uniform int n_edge_vertices;

out lowp vec2 uv;
out highp vec4 shadow_coords;

void main() {
  int row = gl_VertexID / n_edge_vertices;
  int col = gl_VertexID - (row * n_edge_vertices);
  int geometry_id = 0;
  float tile_width = (bounds[geometry_id].z - bounds[geometry_id].x) / float(n_edge_vertices - 1);
  float tile_height = (bounds[geometry_id].w - bounds[geometry_id].y) / float(n_edge_vertices - 1);

  highp vec3 pos_wrt_cam = vec3(float(col) * tile_width + bounds[geometry_id].x,
                        float(n_edge_vertices - row - 1) * tile_width + bounds[geometry_id].y,
                        height * 65536.0 * 0.125 - camera_position.z);

  highp vec3 pos_wrt_sun = vec3(float(col) * tile_width + bounds[geometry_id].x + camera_position.x - sun_position.x,
                            float(n_edge_vertices - row - 1) * tile_width + bounds[geometry_id].y + camera_position.y - sun_position.y,
                            height * 65536.0 * 0.125 - sun_position.z);

  gl_Position = matrix * vec4(pos_wrt_cam, 1);
  shadow_coords = shadow_matrix * vec4(pos_wrt_sun, 1);

  uv = vec2(float(col) / float(n_edge_vertices - 1), float(row) / float(n_edge_vertices - 1));
}

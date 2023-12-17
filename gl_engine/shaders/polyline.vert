layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_tangent;
layout(location = 2) in highp vec3 a_next_position;

uniform highp mat4 matrix; // view projection matrix
uniform highp vec3 camera_position;
uniform highp float width;

flat out int vertex_id;

void main() {
  vertex_id = gl_VertexID;

  vec3 world_pos = a_position - camera_position; // should be done on cpu

  /* the sign of the normal is flipped for vertices that are under the 
  original points, so we displace in the correct direction. */

  const vec3 up = vec3(0,0,1);

#if 1
  vec3 view_dir = normalize(camera_position - a_position);
#else
  vec3 view_dir = up;
#endif

#if 1
  vec3 displacement = cross(a_tangent, view_dir);
  world_pos += displacement * width;
  gl_Position = matrix * vec4(world_pos, 1);
#else

  mat3 rotation_matrix = mat3(matrix);

  vec3 screen_space_tangent = rotation_matrix * a_tangent;

  vec4 screen_space_pos = matrix * vec4(world_pos, 1);
  
  vec3 normal = vec3(-screen_space_tangent.y, screen_space_tangent.x, 0);

  gl_Position = screen_space_pos + vec4(normal, 0) * width;
#endif


}
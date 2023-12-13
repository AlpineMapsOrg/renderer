layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_normal;

uniform highp mat4 matrix; // view projection matrix
uniform highp vec3 camera_position;
uniform highp float width;

flat out int vertex_id;

void main() {
  vec3 world_pos = a_position - camera_position; // should be done on cpu

  /* the sign of the normal is flipped for vertices that are under the 
  original points, so we displace in the correct direction. */

  vec3 view_dir = normalize(camera_position - a_position);


  vec3 normal = a_normal;

  vec3 displacement = cross(normal, view_dir);

  world_pos += displacement * width;

  vec4 screen_space_pos = matrix * vec4(world_pos, 1);
// TODO: vertex expansion here

  // depth test?

  // data_texture can be TEXTURE_1D
  // texture() texelLoad()
  //vec3 direction = normalize(a_position - data_texture[gl_VertexID + 1])

  gl_Position = screen_space_pos;
  vertex_id = gl_VertexID;
}
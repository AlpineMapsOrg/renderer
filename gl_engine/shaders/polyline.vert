layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_normal;

uniform highp mat4 matrix;
uniform highp vec3 camera_position;
uniform highp float width;

void main() {
  vec3 world_pos = a_position - camera_position; // should be done on cpu

  /* the sign of the normal is flipped for vertices that are under the 
  original points, so we displace in the correct direction. */

  vec3 view_dir = normalize(camera_position - a_position);

#if 0
  vec3 normal = a_normal;
#else
  //vec3 normal = mat3(transpose(inverse(matrix))) * a_normal;
  vec3 normal = vec3(matrix * vec4(a_normal, 1)); // this works pretty much
#endif


  vec3 displacement = cross(normal, view_dir);

  world_pos += displacement * width;

  gl_Position = matrix * vec4(world_pos, 1);
}
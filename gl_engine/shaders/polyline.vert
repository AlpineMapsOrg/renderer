layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_normal;

uniform highp mat4 matrix;
uniform highp vec3 camera_position;

void main() {
  vec3 world_pos = a_position - camera_position; // should be done on cpu

#if 1
  /* the sign of the normal is flipped for vertices that are under the 
  original points, so we displace in the correct direction. */

  vec3 view_dir = normalize(camera_position - a_position);

  const float displacement_factor = 10.0;
  vec3 displacement = cross(a_normal, view_dir);

  world_pos = world_pos + displacement * displacement_factor;
#endif

  gl_Position = matrix * vec4(world_pos, 1);
}
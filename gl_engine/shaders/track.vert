layout(location = 0) in highp vec3 a_position;
uniform highp mat4 matrix;
uniform highp vec3 camera_position;

void main() {
  vec3 world_pos = a_position - camera_position;
  gl_Position = matrix * vec4(world_pos, 1);
}
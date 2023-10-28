layout(location = 0) in vec3 a_position;
uniform highp mat4 matrix;
uniform highp vec3 camera_position;

void main() {
  //gl_Position = matrix * vec4(a_position, 1.0);
  gl_Position = vec4(a_position, 1.0);
}
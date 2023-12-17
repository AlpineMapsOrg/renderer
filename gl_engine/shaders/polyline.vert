
#include "overlay_steepness.glsl"

layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_tangent;
layout(location = 2) in highp vec3 a_next_position;

uniform highp mat4 matrix; // view projection matrix
uniform highp vec3 camera_position;
uniform highp float width;
uniform highp float aspect;

flat out int vertex_id;
out vec3 color;

#define SCREEN_SPACE 1

void main() {
  vertex_id = gl_VertexID;

  // could be done on cpu
  vec3 position = a_position - camera_position; 
  vec3 next = a_next_position - camera_position;

  vec3 view_dir = normalize(camera_position - a_position);

  vec3 dir = next - position;
  float horizontal_distance = length(vec3(dir.xy, 0));
  float elevation_difference = abs(next.z - position.z);
  float gradient = elevation_difference / horizontal_distance;

  color = gradient_color(clamp(gradient, 0, 1));

#if (SCREEN_SPACE == 0)

  /* the sign of the tangent is flipped for vertices that are under the 
  original points, so we displace in the correct direction. */

  color = vec3(0,1,0);
  vec3 offset = cross(a_tangent, view_dir);
  gl_Position = matrix * vec4(position + offset * width, 1);

#else /* (SCREEN_SPACE == 1) */

  //color = vec3(1,0,0);

  vec2 aspect_vec = vec2(aspect, 1);

  vec4 current_projected = matrix * vec4(position, 1);
  vec4 next_projected = matrix * vec4(next, 1);

  vec2 current_screen = current_projected.xy / current_projected.w * aspect_vec;
  vec2 next_screen = next_projected.xy / next_projected.w * aspect_vec;

  float orientation = (gl_VertexID % 2 == 0) ? +1 : -1;

  vec2 direction = normalize(next_screen - current_screen);
  vec2 normal = vec2(-direction.y, direction.x);
  normal.x /= aspect;

  vec4 offset = vec4(normal * orientation, 0, 0);
  gl_Position = current_projected + offset * width;

#endif
}
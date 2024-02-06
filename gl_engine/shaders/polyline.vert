
#include "overlay_steepness.glsl"

layout(location = 0) in highp vec3 a_position;
//layout(location = 1) in highp vec3 a_tangent;
//layout(location = 2) in highp vec3 a_next_position;

uniform highp mat4 matrix; // view projection matrix
uniform highp vec3 camera_position;
uniform highp float width;
uniform highp float aspect;
uniform highp bool visualize_steepness;
uniform highp sampler2D texin_track;

flat out int vertex_id;
out vec3 color;

#define SCREEN_SPACE 1

void main() {
  vertex_id = gl_VertexID;

#if 1
  // edge case handled by ClampToEdge
  uint id = gl_VertexID / 2;
  highp vec3 tex_position = texelFetch(texin_track, ivec2(id, 0), 0).xyz;
  highp vec3 tex_next_position = texelFetch(texin_track, ivec2(id + 1, 0), 0).xyz;

  // could be done on cpu
  vec3 position = tex_position - camera_position;
  vec3 next = tex_next_position - camera_position;

  vec3 view_dir = normalize(camera_position - tex_position);

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
#else

  uint id = gl_VertexID / 2;
  highp vec3 tex_position = texelFetch(texin_track, ivec2(id, 0), 0).xyz;

  vec3 position = a_position - camera_position;

  //vec3 position = tex_position - camera_position;

  gl_Position = matrix * vec4(position, 1);
#endif

}
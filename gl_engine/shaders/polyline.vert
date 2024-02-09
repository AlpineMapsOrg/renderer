
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

#if 0
// x0, x1: cone start and end
// r0, r1: sphere cap radii
//
void bounding_quad(in vec3 x0, in float r0 in vec3 x1, float r1, out vec3 p0, out vec3 p1, out vec3 p2, out vec3 p3) {

  vec3 d = x1 - x0;
  vec3 e = camera_position;
  vec3 d0 = e - x0;
  vec3 d1 = e - x1;

  // no idea if this is correct
  vec3 u  = cross(d, d0) * (1.0 / length(cross(d, d0)));
  vec3 v0 = cross(u, d0) * (1.0 / length(cross(u, d0)));
  vec3 v1 = cross(u, d1) * (1.0 / length(cross(u, d1)));

  // scaling factor
  float s0 = 1;
  float s1 = 1;

  float r0_hat = length(d0) * s0;
  float r1_hat = length(d1) * s1;


  p0 = x0 + r0_hat * v0;
  p1 = x0 - r0_hat * v0;
  p2 = x1 + r1_hat * v1;
  p2 = x1 - r1_hat * v1;

}
#endif

void main() {
  // the closest gpx point to the vertex
  vertex_id = (gl_VertexID / 3) - (gl_VertexID / 6);

#if 0

  highp vec3 tex_position = texelFetch(texin_track, ivec2(vertex_id, 0), 0).xyz;
  highp vec3 tex_next_position = texelFetch(texin_track, ivec2(vertex_id + 1, 0), 0).xyz;

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
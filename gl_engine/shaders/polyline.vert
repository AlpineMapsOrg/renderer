
#include "overlay_steepness.glsl"

layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_direction;
layout(location = 2) in highp vec3 a_offset;

uniform highp mat4 matrix; // view projection matrix
uniform highp vec3 camera_position;
uniform highp float width;
uniform highp float aspect;
uniform highp bool visualize_steepness;
uniform highp sampler2D texin_track;

flat out int vertex_id;
out vec3 color;

#define METHOD 2

void main() {
  // the closest gpx point to the vertex
  vertex_id = (gl_VertexID / 3) - (gl_VertexID / 6);

#if (METHOD == 1)
  uint id = (gl_VertexID / 2) - (gl_VertexID / 4);
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

  float orientation = a_offset.z;

  vec2 direction = normalize(next_screen - current_screen);
  vec2 normal = vec2(-direction.y, direction.x);
  normal.x /= aspect;

  vec4 offset = vec4(normal * orientation, 0, 0);
  gl_Position = current_projected + offset * width;


#elif (METHOD == 2)

  // Advanced Rendering of Line Data with Ambient Occlusion and Transparency
  // 1. fallunterscheidung, which vertex position do we need to calculate? p0, p1, p2 or p3?
  //    we already now if we are top or bottom from a_offset, only need to compute if start or end
  //    of rounded cone
  // 2. find track points that make up the endpoints of our rounded cone (x0, x1)
  // 3. calculate d, d0, d1
  // 4. calculate u, v0, v1

  vec3 e = camera_position;
  vec3 x0 = a_position - camera_position;

  // main axis of the rounded cone
  vec3 d = a_direction;

  vec3 d0 = camera_position - a_position;

  // orthogonal to main axis
  vec3 u_hat = normalize(cross(d, d0));

  vec3 v0 = normalize(cross(u_hat, d0));

  float r0 = 7; // cone end cap radius

  float t0 = sqrt(dot(d0, d0) - (r0 * r0));

  //float r0_hat = length(d0) * (r0 / t0);

  float r0_prime = r0;

  // direction of offset
  float f;

  if (gl_VertexID == 0 || gl_VertexID == 1 || gl_VertexID == 3) {
    f = +1;
  } else {
    f = -1;
  }

  vec3 p0 = x0 + (f * v0 * r0_prime);

  float r0_double_prime = r0;

  vec3 position = p0 + u_hat * a_offset.z * r0_double_prime;



  gl_Position = matrix * vec4(position, 1);


#elif(METHOD == 3)
  // naive orientation towards camera
  // does not contain the whole capsule
  const float r = 15;
  vec3 position = a_position - camera_position;
  vec3 view_dir = normalize(camera_position - a_position);
  vec3 displacement = cross(a_direction, view_dir);
  position += displacement * r * a_offset.z;
  gl_Position = matrix * vec4(position, 1);
#else

  vec3 tex_position       = texelFetch(texin_track, ivec2(vertex_id + 0, 0), 0).xyz;
  vec3 tex_next_position  = texelFetch(texin_track, ivec2(vertex_id + 1, 0), 0).xyz;

  vec3 next_position    = tex_next_position - camera_position;
  vec3 current_position = tex_position - camera_position;

  // why is this not the same value?
  vec3 position = a_position - camera_position;

  vec3 direction = normalize(current_position - next_position);
  vec3 view_dir = normalize(camera_position - tex_position);
  vec3 normal = cross(view_dir, direction);


  //position += a_offset * 15.0;
  position += normal * a_offset.z * 15.0;



  gl_Position = matrix * vec4(position, 1);
#endif

}
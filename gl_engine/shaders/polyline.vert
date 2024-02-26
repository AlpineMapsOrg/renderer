
#include "overlay_steepness.glsl"

layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_direction;
layout(location = 2) in highp vec3 a_offset;
//layout(location = 3) in highp vec3 a_metadata; // data like speed, vertical speed, etc...

uniform highp mat4 view;
uniform highp mat4 proj;
uniform highp vec3 camera_position;
uniform highp float width;
uniform highp float aspect;
uniform bool visualize_steepness;
uniform highp sampler2D texin_track;

flat out highp int vertex_id;
flat out highp float radius;

#define METHOD 2

void main() {

  highp mat4 matrix = proj * view;


  // the closest gpx point to the vertex
  //vertex_id = (gl_VertexID / 3) - (gl_VertexID / 6);
  vertex_id = int(a_offset.y);

  radius = width;

#if (METHOD == 1)
  int id = (gl_VertexID / 2) - (gl_VertexID / 4);
  highp vec3 tex_position = texelFetch(texin_track, ivec2(id, 0), 0).xyz;
  highp vec3 tex_next_position = texelFetch(texin_track, ivec2(id + 1, 0), 0).xyz;

  // could be done on cpu
  highp vec3 position = tex_position - camera_position;
  highp vec3 next = tex_next_position - camera_position;

  highp vec3 view_dir = normalize(camera_position - tex_position);

  highp vec2 aspect_vec = vec2(aspect, 1);

  highp vec4 current_projected = matrix * vec4(position, 1);
  highp vec4 next_projected = matrix * vec4(next, 1);

  highp vec2 current_screen = current_projected.xy / current_projected.w * aspect_vec;
  highp vec2 next_screen = next_projected.xy / next_projected.w * aspect_vec;

  highp float orientation = a_offset.z;

  highp vec2 direction = normalize(next_screen - current_screen);
  highp vec2 normal = vec2(-direction.y, direction.x);
  normal.x /= aspect;

  highp vec4 offset = vec4(normal * orientation, 0, 0);
  gl_Position = current_projected + offset * width;


#elif (METHOD == 2)

  int shading_method = 0;

  highp vec3 e = camera_position;
  highp vec3 x0 = a_position - camera_position;

  // main axis of the rounded cone
  highp vec3 d = a_direction;

  highp vec3 d0 = camera_position - a_position;

  // orthogonal to main axis
  highp vec3 u_hat = normalize(cross(d, d0));

  highp vec3 v0 = normalize(cross(u_hat, d0));


  highp float r0 = radius; // cone end cap radius

  highp float t0 = sqrt(dot(d0, d0) - (r0 * r0));

  // TODO
  //float r0_prime = length(d0) * (r0 / t0);
  highp float r0_prime = r0;


  // TODO:
  highp float r0_double_prime = r0;

  highp vec3 p0 = x0 + (a_offset.x * v0 * r0_prime);

  highp vec3 position = p0 + u_hat * a_offset.z * r0_double_prime;

  //color = vec3(1,0,1);

  gl_Position = matrix * vec4(position, 1);

#elif(METHOD == 3)
  // naive orientation towards camera
  // does not contain the whole capsule
  const highp float r = 15;
  highp vec3 position = a_position - camera_position;
  highp vec3 view_dir = normalize(camera_position - a_position);
  highp vec3 displacement = cross(a_direction, view_dir);
  position += displacement * r * a_offset.z;
  gl_Position = matrix * vec4(position, 1);
#else

  highp vec3 position = a_position - camera_position;

  position += vec3(0,0,1) * a_offset.z * 15.0;

  gl_Position = matrix * vec4(position, 1);
#endif

}

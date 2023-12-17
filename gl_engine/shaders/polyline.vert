layout(location = 0) in highp vec3 a_position;
layout(location = 1) in highp vec3 a_tangent;
layout(location = 2) in highp vec3 a_next_position;

uniform highp mat4 matrix; // view projection matrix
uniform highp vec3 camera_position;
uniform highp float width;
uniform highp float aspect;

flat out int vertex_id;
out vec3 color;

void main() {
  vertex_id = gl_VertexID;
  color = vec3(0.69, 0.12, 0.09);

  // could be done on cpu
  vec3 position = a_position - camera_position; 
  vec3 next = a_next_position - camera_position;

  /* the sign of the normal is flipped for vertices that are under the 
  original points, so we displace in the correct direction. */

  const vec3 up = vec3(0,0,1);

#if 1
  vec3 view_dir = normalize(camera_position - a_position);
#else
  vec3 view_dir = up;
#endif

#if 0
  vec3 offset = cross(a_tangent, view_dir);

  gl_Position = matrix * vec4(position + offset * width, 1);

#if 1
  if (gl_VertexID % 2 == 0) {
    color = vec3(1,0,0);
  } else {
    color = vec3(0,1,0);
  }
#endif

#else

  vec2 aspect_vec = vec2(aspect, 1.0f);

  vec4 current_projected = matrix * vec4(position, 1);
  vec4 next_projected = matrix * vec4(next, 1);

  vec2 current_screen = current_projected.xy / current_projected.w * aspect_vec;
  vec2 next_screen = next_projected.xy / next_projected.w * aspect_vec;


  float orientation = -1;

  if (gl_VertexID % 2 == 0) {
    orientation = +1;
    color = vec3(1,0,0);
  } else {
    orientation = -1;
    color = vec3(0,1,0);
  }

  vec2 dir = normalize(next_screen - current_screen);

  vec2 perp = vec2(-dir.y, dir.x);

  vec4 offset = vec4(perp * orientation, 0, 1);
  gl_Position = current_projected + offset * width;


  //gl_Position = current_projected;
#endif
}
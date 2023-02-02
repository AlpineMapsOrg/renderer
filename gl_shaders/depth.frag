uniform highp vec3 camera_position;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

void main() {
   highp vec3 origin = vec3(camera_position);
   highp float dist = length(pos_wrt_cam);
   highp vec3 dist_vec = vec3(dist);
   highp vec3 ray_direction = pos_wrt_cam / dist;

   highp float near = 10.0;
   highp float far = 100.0;
   highp float normalized_depth = gl_FragCoord.z * 2.0 - 1.0;
   highp float linear_depth = (2.0 * near * far) / (far + near - normalized_depth * (far - near));

   out_Color = vec4(vec3(dist / 1000.0, dist / 10000.0, dist / 100000.0), 1.0);
}

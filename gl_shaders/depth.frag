uniform highp vec3 camera_position;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

void main() {
   highp float dist = length(pos_wrt_cam);
   out_Color = vec4(log(dist)/13.0, 0, 0, 0);
}

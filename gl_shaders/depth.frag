uniform highp vec3 camera_position;
in highp vec3 pos_wrt_cam;
out highp float out_Color;

void main() {
   highp float dist = length(pos_wrt_cam);

   out_Color = dist;
}

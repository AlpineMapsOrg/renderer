uniform highp vec3 camera_position;
uniform sampler2D texture_sampler;
in lowp vec2 uv;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

void main() {
   highp vec3 origin = vec3(camera_position);
   highp vec4 ortho = texture(texture_sampler, uv);
   highp float dist = length(pos_wrt_cam);
   highp vec3 ray_direction = pos_wrt_cam / dist;
   
   vec3 shading_point = camera_position + pos_wrt_cam;
   highp vec3 light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, vec3(ortho));
   out_Color = vec4(light_through_atmosphere, 1.0);
}

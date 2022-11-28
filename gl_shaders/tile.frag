uniform highp vec3 camera_position;
uniform sampler2D texture_sampler;
in lowp vec2 uv;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

highp vec3 calculate_atmospheric_light(highp vec3 ray_origin, highp vec3 ray_direction, highp float ray_length, highp vec3 original_colour, int n_numerical_integration_steps);

void main() {
   highp vec3 origin = vec3(camera_position);
   highp vec4 ortho = texture(texture_sampler, uv);
   highp float dist = length(pos_wrt_cam);
   highp vec3 ray_direction = pos_wrt_cam / dist;

   highp vec3 light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, vec3(ortho), 10);
   out_Color = vec4(light_through_atmosphere, 1.0);
}

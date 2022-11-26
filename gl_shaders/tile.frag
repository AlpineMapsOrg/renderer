uniform highp vec3 camera_position;
uniform sampler2D texture_sampler;
uniform sampler2D atmosphere_lookup_sampler;
in lowp vec2 uv;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

void main() {
   highp vec3 origin = vec3(camera_position);
   highp vec4 ortho = texture(texture_sampler, uv);
   highp float dist = length(pos_wrt_cam);
   highp vec3 ray_direction = pos_wrt_cam / dist;
   
   vec3 shading_point = camera_position + pos_wrt_cam;
//   highp vec3 inscattering = calculate_atmospheric_light(camera_position.z / 1000.0, ray_direction, dist / 1000.0, (camera_position.z + pos_wrt_cam.z) / 1000.0) / 100;
//   highp vec3 light_through_atmosphere = rm_calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, vec3(ortho));
   highp vec3 light_through_atmosphere = lu_calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, vec3(ortho), atmosphere_lookup_sampler);

//   highp vec3 inscattering = exp(- 0.1 * optical_depth(camera_position.z/1000.0, ray_direction, (shading_point.z)/1000.0, dist / 1000.) * scattering_coefficients()) / 2;

//   highp float inscattering = optical_depth(shading_point.z / 1000.0, -ray_direction, camera_position.z/1000.0, length(shading_point / 1000.0 - camera_position/1000.0)) / 100;
//   highp float inscattering = optical_depth(shading_point.z / 1000.0, -ray_direction, camera_position.z/1000.0, dist / 1000.) / 100;
//   highp float inscattering = optical_depth(camera_position.z / 1000.0, ray_direction, camera_position.z / 1000.0, 0.0) + 0.5;
//   highp float inscattering = rm_optical_depth(camera_position / 1000.0, ray_direction, 0) + 0.5;
//   highp float inscattering = optical_depth2(camera_position / 1000.0, ray_direction, 0) + 0.5;
//   highp float inscattering = 1.0 / 0.0 - 1.0;

//   highp float inscattering = optical_depth(camera_position.z/1000.0, vec3(0.0, 0.0, 1.0), 100.0 + camera_position.z/1000.0, 100.) / 1;
//   highp float inscattering = rm_optical_depth(camera_position / 1000.0, vec3(0.0, 0.0, 1.0), 100) / 1;

//   out_Color = ortho * (1.0 - inscattering) * 0.0 + vec4(inscattering, inscattering, inscattering, 1.0);
//   out_Color = ortho * (1.0 - inscattering.x) * 0.0 + vec4(inscattering, 1.0);
   out_Color = vec4(light_through_atmosphere, 1.0);
   //gl_FragDepth = gl_FragCoord.z;
}

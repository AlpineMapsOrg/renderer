#line 1
uniform highp vec3 camera_position;
uniform highp mat4 inversed_projection_matrix;
uniform highp mat4 inversed_view_matrix;
in lowp vec2 texcoords;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

const highp float infinity = 1.0 / 0.0;

highp vec3 unproject(vec2 normalised_device_coordinates) {
   highp vec4 unprojected = inversed_projection_matrix * vec4(normalised_device_coordinates, 1.0, 1.0);
   highp vec4 normalised_unprojected = unprojected / unprojected.w;
   
   return normalize(vec3(inversed_view_matrix * normalised_unprojected));
}

void main() {
   highp vec3 origin = vec3(camera_position);
   highp vec3 ray_direction = unproject(texcoords * 2.0 - 1.0);
   highp float ray_length = 2000;
   highp vec3 background_colour = vec3(0.0, 0.0, 0.0);
   if (ray_direction.z < 0) {
       ray_length = min(ray_length, -(origin.z * 0.001) / ray_direction.z);
   }
   highp vec3 light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, ray_length, background_colour, 250);

   out_Color = vec4(light_through_atmosphere, 1.0);
}

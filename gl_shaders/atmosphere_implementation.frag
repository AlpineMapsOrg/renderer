
const int n_atmospheric_marching_steps = 10;
const highp vec3 wavelengths = vec3(700, 530, 440);
highp vec3 scattering_coefficients() {
    return pow(400 / wavelengths, vec3(4.0, 4.0, 4.0)) * 0.018;
}

highp float density_at_height(highp float height_msl) {
   return exp(-height_msl * 0.13);
}

highp float optical_depth(highp float origin_height, highp vec3 direction, highp float end_height, highp float distance) {
    // integral of density along a direction.
    highp float w = 0.13;
    highp float dot = direction.z;//dot(direction, vec3(0.0, 0.0, 1.0));
    if (abs(dot) < 0.001)
        return distance;
    if (distance < 0.01)
        return 0.0;
    highp float m2ln_dot = -2 * log(dot);
    highp float oneOverW = 1.0 / w;
    return oneOverW * (exp(-w * origin_height) - exp(-w * end_height)) * distance / (end_height - origin_height);
}

highp float optical_depth2(vec3 ray_origin, vec3 ray_dir, float ray_length) {
    return optical_depth(ray_origin.z, ray_dir, ray_origin.z + ray_dir.z * ray_length, ray_length);
}

highp float rm_optical_depth(vec3 ray_origin, vec3 ray_dir, float ray_length) {
    const int n_optical_depth_steps = 10;
    float density_sample_point = ray_origin.z;
    float step_size = ray_length / (n_optical_depth_steps - 1);
    float optical_depth = 0.0;
    for (int i = 0; i < n_optical_depth_steps; i++) {
        float local_density = density_at_height(density_sample_point);
        optical_depth += local_density;
        density_sample_point += ray_dir.z * step_size;
    }
    return optical_depth * step_size;
}

// computation via direction and length for ray end height is numerically unstable, hence the extra param
highp vec3 calculate_atmospheric_light(highp float origin_height, highp vec3 ray_direction, highp float ray_length, highp float ray_end_height) {
   highp vec2 marching_position = vec2(0.0, origin_height);
   highp float one_over_n_steps = 1.0 / float(n_atmospheric_marching_steps);
   highp vec2 marching_step = vec2(length(ray_direction.xy) * ray_length * one_over_n_steps,
                                   ray_direction.z * ray_length * one_over_n_steps);
   highp float marching_step_lenght = ray_length / n_atmospheric_marching_steps;// length(marching_step);
   highp float in_scattered_light = 0.0;
   for (int i = 0; i < n_atmospheric_marching_steps; i++) {
      float sun_ray_optical_depth = optical_depth(marching_position.y, vec3(0, 0, 1), 100.0, 100 - marching_position.y);
      float view_ray_optical_depth = optical_depth(marching_position.y, ray_direction, ray_end_height, marching_step_lenght * i);
      
      float transmittance = exp(-0.1 * (sun_ray_optical_depth + view_ray_optical_depth));
      
      float local_density = density_at_height(marching_position.y);
      
//       in_scattered_light += marching_step_lenght_in_km * 0.01 * transmittance;
      in_scattered_light += /*local_density * */transmittance * marching_step_lenght;
      marching_position += marching_step;
   }
   return vec3(in_scattered_light, in_scattered_light, in_scattered_light);
}

highp vec3 rm_calculate_atmospheric_light_orig(vec3 ray_origin, vec3 ray_direction, float ray_length, vec3 original_colour) {
   highp vec3 marching_position = ray_origin;
   float step_size = ray_length / float(n_atmospheric_marching_steps - 1);
   highp vec3 in_scattered_light = vec3(0.0, 0.0, 0.0);

   for (int i = 0; i < n_atmospheric_marching_steps; i++) {
      float sun_ray_optical_depth = rm_optical_depth(marching_position, vec3(0, 0, 1), 100.0);
      float view_ray_optical_depth = rm_optical_depth(marching_position, -ray_direction, step_size * i);
      vec3 transmittance = exp(-(sun_ray_optical_depth + view_ray_optical_depth) * scattering_coefficients());
      float local_density = density_at_height(marching_position.z);

      in_scattered_light += local_density * transmittance * step_size * scattering_coefficients();
      marching_position += ray_direction * step_size;

   }
   float view_ray_optical_depth = rm_optical_depth(ray_origin, ray_direction, ray_length);
   vec3 transmittance = exp(-(view_ray_optical_depth) * scattering_coefficients());

   return in_scattered_light + transmittance * original_colour;
}

highp vec3 rm_calculate_atmospheric_light(vec3 ray_origin, vec3 ray_direction, float ray_length, vec3 original_colour) {
   highp vec3 marching_position = ray_origin;
   float step_size = ray_length / float(n_atmospheric_marching_steps - 1);
   highp vec3 in_scattered_light = vec3(0.0, 0.0, 0.0);

   for (int i = 0; i < n_atmospheric_marching_steps; i++) {
//      float sun_ray_optical_depth = optical_depth(marching_position.z, vec3(0, 0, 1), 100.0, 100.0 - marching_position.z);
      float sun_ray_optical_depth = rm_optical_depth(marching_position, vec3(0, 0, 1), 100.0);

      float view_ray_optical_depth = optical_depth(marching_position.z, -ray_direction, ray_origin.z, length(ray_origin - marching_position));
//      float view_ray_optical_depth = rm_optical_depth(ray_origin, ray_direction, step_size * i);

      vec3 transmittance = exp(-(sun_ray_optical_depth + view_ray_optical_depth) * scattering_coefficients());
      float local_density = density_at_height(marching_position.z);

      in_scattered_light += local_density * transmittance * step_size * scattering_coefficients();
      marching_position += ray_direction * step_size;

   }
   float view_ray_optical_depth = optical_depth(ray_origin.z, ray_direction, ray_origin.z + ray_direction.z * ray_length, ray_length);
   vec3 transmittance = exp(-(view_ray_optical_depth) * scattering_coefficients());

   return in_scattered_light + transmittance * original_colour;
}

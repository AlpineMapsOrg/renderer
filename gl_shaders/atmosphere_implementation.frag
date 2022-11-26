
const highp float atmosphere_height = 100.0;
const highp float earth_radius = 6371.0;
const int n_atmospheric_marching_steps = 100;
const int n_optical_depth_steps = 20;
//const highp vec3 wavelengths = vec3(700, 530, 440);
const highp vec3 wavelengths = vec3(700, 560, 440);
highp vec3 scattering_coefficients() {
    return pow(400 / wavelengths, vec3(4.0, 4.0, 4.0)) * 0.04;
}

highp float density_at_height(highp float height_msl) {
   return exp(-height_msl * 0.13);
}

highp float an_optical_depth(highp float origin_height, highp vec3 direction, highp float distance) {
    highp float end_height = origin_height + direction.z * distance;
    // integral of density along a direction.
    highp float w = 0.13;
    highp float dot = direction.z;//dot(direction, vec3(0.0, 0.0, 1.0));
    if (abs(dot) < 0.001)
        return distance * 0.5 * (density_at_height(origin_height) + density_at_height(end_height));
    if (distance < 0.01)
        return 0.0;
    highp float m2ln_dot = -2 * log(dot);
    highp float oneOverW = 1.0 / w;
    return oneOverW * (exp(-w * origin_height) - exp(-w * end_height)) * distance / (end_height - origin_height);
}

//// flat world old
//highp float rm_optical_depth(vec3 ray_origin, vec3 ray_dir, float ray_length) {
//    float density_sample_point = ray_origin.z;
//    float step_size = ray_length / (n_optical_depth_steps - 1);
//    float optical_depth = 0.0;
//    for (int i = 0; i < n_optical_depth_steps; i++) {
//        float local_density = density_at_height(density_sample_point);
//        optical_depth += local_density;
//        density_sample_point += ray_dir.z * step_size;
//    }
//    return optical_depth * step_size;
//}
// flat world adaptive sampling
highp float rm_optical_depth(vec3 ray_origin, vec3 ray_dir, float ray_length) {
    float density_sample_point = ray_origin.z;
    float step_size = ray_length / (n_optical_depth_steps - 1);
    float optical_depth = 0.0;
    float min_step = max(0.5, 10 * ray_dir.z);
    for (float t = 0; t < ray_length;) {
        float height1 = density_sample_point;
        float step_length = min(max(min_step, height1/10), ray_length - t);
        t += step_length;
        density_sample_point = ray_origin.z + t * ray_dir.z;
        float height2 = density_sample_point;

        optical_depth += step_length * 0.5 * (density_at_height(height1) + density_at_height(height2));
    }
    return optical_depth;
}

//// spherical world
//highp float rm_optical_depth(vec3 ray_origin, vec3 ray_dir, float ray_length) {
//    highp vec2 density_sample_point = vec2(0, ray_origin.z + earth_radius);
//    highp vec2 density_sample_dir = vec2(sqrt(max(0.0, 1 - ray_dir.z*ray_dir.z)), ray_dir.z);

//    float step_size = ray_length / (n_optical_depth_steps - 1);
//    float optical_depth = 0.0;
//    for (int i = 0; i < n_optical_depth_steps; i++) {
//        float height = length(density_sample_point) - earth_radius;
//        float local_density = density_at_height(height);
//        optical_depth += local_density;
//        density_sample_point += density_sample_dir * step_size;
//    }
//    return optical_depth * ray_length / float(n_optical_depth_steps);
//}

//highp float lu_optical_depth(highp vec3 ray_origin, highp vec3 ray_dir, highp float ray_length, sampler2D lookup_sampler) {
//    highp float start_height = ray_origin.z;

//    highp vec2 density_sample_start = vec2(0, ray_origin.z + earth_radius);
//    highp vec2 density_sample_dir = vec2(sqrt(max(0.0, 1 - ray_dir.z*ray_dir.z)), ray_dir.z);
//    highp vec2 density_sample_destination = density_sample_start + ray_length * density_sample_dir;
//    highp float end_radius = length(density_sample_destination);
//    highp float end_height = end_radius - earth_radius;
//    highp vec2 end_up_vector = density_sample_destination / end_radius;
//    highp float end_relativ_ray_dir_z = dot(end_up_vector, density_sample_dir);

//    start_height = max(0, min(atmosphere_height, start_height));
//    end_height = max(0, min(atmosphere_height, end_height));
//    start_height = start_height / atmosphere_height; // linear
//    end_height = end_height / atmosphere_height;

////    start_height = max(0.01, start_height);
////    end_height = max(0.01, end_height);
////    start_height = (log(start_height) - log(0.01))/(log(atmosphere_height) - log(0.01));
////    end_height = (log(end_height) - log(0.01))/(log(atmosphere_height) - log(0.01));
//////    start_height = log(start_height * atmosphere_height) / (log(atmosphere_height) - log(0.01)); // logarithmic
//////    end_height = log(end_height * atmosphere_height) / (log(atmosphere_height) - log(0.01));

//    highp float start_lookup_dir = (ray_dir.z * 0.5 + 0.5);
//    highp float end_lookup_dir = (end_relativ_ray_dir_z * 0.5 + 0.5);

////    return texture(lookup_sampler, vec2(start_height, lookup_ray_dir_z)).r;
//    return abs(texture(lookup_sampler, vec2(end_height, end_lookup_dir)).r - texture(lookup_sampler, vec2(start_height, start_lookup_dir)).r);
//}


highp vec3 rm_calculate_atmospheric_light_orig(vec3 ray_origin, vec3 ray_direction, float ray_length, vec3 original_colour) {
   highp vec3 marching_position = ray_origin;
   float step_size = ray_length / float(n_atmospheric_marching_steps - 1);
   highp vec3 in_scattered_light = vec3(0.0, 0.0, 0.0);

   for (int i = 0; i < n_atmospheric_marching_steps; i++) {
      float sun_ray_optical_depth = rm_optical_depth(marching_position, vec3(0, 0, 1), atmosphere_height);
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
//       float sun_ray_optical_depth = optical_depth(marching_position.z, vec3(0, 0, 1), atmosphere_height, atmosphere_height - marching_position.z);
      float sun_ray_optical_depth = rm_optical_depth(marching_position, vec3(0, 0, 1), atmosphere_height - marching_position.z);

//      float view_ray_optical_depth = optical_depth(marching_position.z, -ray_direction, ray_origin.z, length(ray_origin - marching_position));
      float view_ray_optical_depth = rm_optical_depth(ray_origin, ray_direction, step_size * i);
//      if (i == n_atmospheric_marching_steps - 1)
//        return vec3(view_ray_optical_depth, view_ray_optical_depth, view_ray_optical_depth)/100;

      vec3 transmittance = exp(-(sun_ray_optical_depth + view_ray_optical_depth) * scattering_coefficients());
      float local_density = density_at_height(marching_position.z);

      in_scattered_light += local_density * transmittance;
      marching_position += ray_direction * step_size;

   }
   float cos_sun = dot(ray_direction, vec3(0.0, 0.0, 1.0));
   float phase_function = 0.75 * (1 + cos_sun * cos_sun);
   in_scattered_light *= (ray_length / n_atmospheric_marching_steps) * scattering_coefficients() * phase_function;
//   float view_ray_optical_depth = optical_depth(ray_origin.z, ray_direction, ray_origin.z + ray_direction.z * ray_length, ray_length);
   float view_ray_optical_depth = rm_optical_depth(ray_origin, ray_direction, ray_length);
   vec3 transmittance = exp(-(view_ray_optical_depth) * scattering_coefficients());

   return in_scattered_light + transmittance * original_colour;
}

highp vec3 lu_calculate_atmospheric_light(vec3 ray_origin, vec3 ray_direction, float ray_length, vec3 original_colour, sampler2D lookup_sampler) {
   highp vec3 marching_position = ray_origin;
   float step_size = ray_length / float(n_atmospheric_marching_steps - 1);
   highp vec3 in_scattered_light = vec3(0.0, 0.0, 0.0);

   for (int i = 0; i < n_atmospheric_marching_steps; i++) {

       float sun_ray_optical_depth = an_optical_depth(marching_position.z, vec3(0, 0, 1), atmosphere_height - marching_position.z);
//       float sun_ray_optical_depth = rm_optical_depth(marching_position, vec3(0, 0, 1), atmosphere_height - marching_position.z);
//      float sun_ray_optical_depth = lu_optical_depth(marching_position, vec3(0, 0, 1),  atmosphere_height - marching_position.z, lookup_sampler);

       float view_ray_optical_depth = an_optical_depth(ray_origin.z, ray_direction, step_size * i);
//       float view_ray_optical_depth = rm_optical_depth(ray_origin, ray_direction, step_size * i);
//     float view_ray_optical_depth = lu_optical_depth(ray_origin, ray_direction, step_size * i, lookup_sampler);

      vec3 transmittance = exp(-(sun_ray_optical_depth + view_ray_optical_depth) * scattering_coefficients());
      float local_density = density_at_height(marching_position.z);

      in_scattered_light += local_density * transmittance;
      marching_position += ray_direction * step_size;

   }
   float cos_sun = dot(ray_direction, vec3(0.0, 0.0, 1.0));
   float phase_function = 0.75 * (1 + cos_sun * cos_sun);
   in_scattered_light *= (ray_length / n_atmospheric_marching_steps) * scattering_coefficients() * phase_function;
   float view_ray_optical_depth = an_optical_depth(ray_origin.z, ray_direction, ray_length);
//   float view_ray_optical_depth = lu_optical_depth(ray_origin, ray_direction, ray_length, lookup_sampler);
//   float view_ray_optical_depth = rm_optical_depth(ray_origin, ray_direction, ray_length);
   vec3 transmittance = exp(-(view_ray_optical_depth) * scattering_coefficients());

   return in_scattered_light + transmittance * original_colour;
}

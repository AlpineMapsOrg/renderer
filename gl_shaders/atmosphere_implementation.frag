const highp float atmosphere_height = 100.0;
// only an even number of steps supported (simpson)

//const highp vec3 wavelengths = vec3(700, 530, 440);
const highp vec3 wavelengths = vec3(700, 560, 440);
highp vec3 scattering_coefficients() {
    return pow(400 / wavelengths, vec3(4.0, 4.0, 4.0)) * 0.04;
}

highp float density_at_height(highp float height_msl) {
    return exp(-height_msl * 0.13);
}

highp float an_optical_depth(highp float origin_height, highp float h_delta, highp float distance) {
    highp float end_height = origin_height + h_delta * distance;
    // integral of density along a direction.
    highp float w = 0.13;
    if (abs(h_delta) < 0.001)
        return distance * 0.5 * (density_at_height(origin_height) + density_at_height(end_height));
    return (1.0 / (w * h_delta)) * (exp(-origin_height * w) - exp(-end_height * w));
}

highp vec3 evaluate_atmopshperic_light(highp float h, highp float h_delta, float dist_from_start) {
    highp float sun_ray_optical_depth = an_optical_depth(h, 1.0, atmosphere_height - h);
    highp float view_ray_optical_depth = an_optical_depth(h, -h_delta, dist_from_start);
    vec3 transmittance = exp(-(sun_ray_optical_depth + view_ray_optical_depth) * scattering_coefficients());
    float local_density = density_at_height(h);
    return local_density * transmittance;
}

// simpson's 1/3 rule
highp vec3 integrate_atmoshpere_light(highp float h_start, highp float h_delta, float ray_length, int n_numerical_integration_steps) {
    highp float h_end = h_start + h_delta * ray_length;
    highp vec3 in_scattered_light = vec3(0.0, 0.0, 0.0);
    in_scattered_light += evaluate_atmopshperic_light(h_start, h_delta, ray_length);
    in_scattered_light += evaluate_atmopshperic_light(h_end, h_delta, ray_length);
    highp vec3 in_scattered_light_2 = vec3(0.0, 0.0, 0.0);
    highp vec3 in_scattered_light_4 = vec3(0.0, 0.0, 0.0);

    for (int i = 1; i < n_numerical_integration_steps; ++i) {
        highp float perc = float(i) / n_numerical_integration_steps;
        highp float dist_from_start = perc * ray_length;
        highp float h = mix(h_start, h_end, perc);
        if (mod(i, 2) < 1)
            in_scattered_light_2 += evaluate_atmopshperic_light(h, h_delta, dist_from_start); // even
        else
            in_scattered_light_4 += evaluate_atmopshperic_light(h, h_delta, dist_from_start); // odd
    }
    highp float step_size = ray_length / n_numerical_integration_steps;
    return (step_size / 3.0) * (in_scattered_light + 2.0 * in_scattered_light_2 + 4.0 * in_scattered_light_4);
}

//// rectangle rule
//highp vec3 integrate_atmoshpere_light(highp float h_start, highp float h_delta, float ray_length, int n_numerical_integration_steps) {
//    highp float h = h_start;
//    float step_size = ray_length / float(n_numerical_integration_steps - 1);
//    highp vec3 in_scattered_light = vec3(0.0, 0.0, 0.0);
//    for (int i = 0; i < n_numerical_integration_steps; i++) {
//        highp float dist_from_start = step_size * i;
//        in_scattered_light += evaluate_atmopshperic_light(h, h_delta, dist_from_start);
//        h += h_delta * step_size;

//    }
//    return (ray_length / n_numerical_integration_steps) * in_scattered_light;
//}

highp vec3 calculate_atmospheric_light(vec3 ray_origin, vec3 ray_direction, float ray_length, vec3 original_colour, int n_numerical_integration_steps) {
    highp vec3 integral = integrate_atmoshpere_light(ray_origin.z, ray_direction.z, ray_length, n_numerical_integration_steps);
    float cos_sun = dot(ray_direction, vec3(0.0, 0.0, 1.0));
    float phase_function = 0.75 * (1 + cos_sun * cos_sun);
    integral *= scattering_coefficients() * phase_function;
    float view_ray_optical_depth = an_optical_depth(ray_origin.z, ray_direction.z, ray_length);
    vec3 transmittance = exp(-(view_ray_optical_depth) * scattering_coefficients());

    return integral + transmittance * original_colour;
}



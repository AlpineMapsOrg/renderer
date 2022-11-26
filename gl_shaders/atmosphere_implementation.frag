const highp float atmosphere_height = 100.0;
const int n_atmospheric_marching_steps = 100;

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
    highp float dot = direction.z;
    if (abs(dot) < 0.001)
        return distance * 0.5 * (density_at_height(origin_height) + density_at_height(end_height));
    if (distance < 0.01)
        return 0.0;
    highp float m2ln_dot = -2 * log(dot);
    highp float oneOverW = 1.0 / w;
    return oneOverW * (exp(-w * origin_height) - exp(-w * end_height)) * distance / (end_height - origin_height);
}


highp vec3 calculate_atmospheric_light(vec3 ray_origin, vec3 ray_direction, float ray_length, vec3 original_colour) {
    highp vec3 marching_position = ray_origin;
    float step_size = ray_length / float(n_atmospheric_marching_steps - 1);
    highp vec3 in_scattered_light = vec3(0.0, 0.0, 0.0);

    for (int i = 0; i < n_atmospheric_marching_steps; i++) {

        float sun_ray_optical_depth = an_optical_depth(marching_position.z, vec3(0, 0, 1), atmosphere_height - marching_position.z);

        float view_ray_optical_depth = an_optical_depth(ray_origin.z, ray_direction, step_size * i);

        vec3 transmittance = exp(-(sun_ray_optical_depth + view_ray_optical_depth) * scattering_coefficients());
        float local_density = density_at_height(marching_position.z);

        in_scattered_light += local_density * transmittance;
        marching_position += ray_direction * step_size;

    }
    float cos_sun = dot(ray_direction, vec3(0.0, 0.0, 1.0));
    float phase_function = 0.75 * (1 + cos_sun * cos_sun);
    in_scattered_light *= (ray_length / n_atmospheric_marching_steps) * scattering_coefficients() * phase_function;
    float view_ray_optical_depth = an_optical_depth(ray_origin.z, ray_direction, ray_length);
    vec3 transmittance = exp(-(view_ray_optical_depth) * scattering_coefficients());

    return in_scattered_light + transmittance * original_colour;
}

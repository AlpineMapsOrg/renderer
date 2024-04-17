/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2024 Patrick Komon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

const atmosphere_height = 100.0;
// only an even number of steps supported (simpson)

const wavelengths = vec3f(700.0, 560.0, 440.0);
//const wavelengths = vec3f(700, 530, 440);

fn scattering_coefficients() -> vec3f {
    return pow(400.0 / wavelengths, vec3(4.0, 4.0, 4.0)) * 0.04;
}

fn density_at_height(height_msl: f32) -> f32 {
    return exp(-height_msl * 0.13);
}

fn an_optical_depth(origin_height: f32, h_delta: f32, distance: f32) -> f32 {
    let end_height = origin_height + h_delta * distance;
    // integral of density along a direction.
    const w = 0.13;
    let density_at_orign = density_at_height(origin_height);
    let density_at_end = density_at_height(end_height);
    if (abs(h_delta) < 0.001) {
        return distance * 0.5 * (density_at_orign + density_at_end);
    }
    return (1.0 / (w * h_delta)) * (density_at_orign - density_at_end);

//    highp float w = 0.13;
//    if (abs(h_delta) < 0.001)
//        return distance * 0.5 * (density_at_height(origin_height) + density_at_height(end_height));
//    return (1.0 / (w * h_delta)) * (exp(-origin_height * w) - exp(-end_height * w));
}

fn evaluate_atmopshperic_light(h: f32, h_delta: f32, dist_from_start: f32) -> vec3f {
    let sun_ray_optical_depth = an_optical_depth(h, 1.0, atmosphere_height - h);
    let view_ray_optical_depth = an_optical_depth(h, -h_delta, dist_from_start);
    let transmittance: vec3f = exp(-(sun_ray_optical_depth + view_ray_optical_depth) * scattering_coefficients());
    let local_density = density_at_height(h);
    return local_density * transmittance;
}


// simpson's 1/3 rule
fn integrate_atmoshpere_light(h_start: f32, h_delta: f32, ray_length: f32, n_numerical_integration_steps: i32) -> vec3f {
    let h_end = h_start + h_delta * ray_length;
    var in_scattered_light = vec3(0.0, 0.0, 0.0);
    in_scattered_light += evaluate_atmopshperic_light(h_start, h_delta, ray_length);
    in_scattered_light += evaluate_atmopshperic_light(h_end, h_delta, ray_length);
    var in_scattered_light_2 = vec3(0.0, 0.0, 0.0);
    var in_scattered_light_4 = vec3(0.0, 0.0, 0.0);

    for (var i = 1; i < n_numerical_integration_steps; i++) {
        let perc = f32(i) / f32(n_numerical_integration_steps);
        let dist_from_start = perc * ray_length;
        let h = mix(h_start, h_end, perc);
        if ((i & 1) == 0) {
            in_scattered_light_2 += evaluate_atmopshperic_light(h, h_delta, dist_from_start); // even
        } else {
            in_scattered_light_4 += evaluate_atmopshperic_light(h, h_delta, dist_from_start); // odd
        }
    }
    let step_size = ray_length / f32(n_numerical_integration_steps);
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

fn calculate_atmospheric_light(ray_origin: vec3f, ray_direction: vec3f, ray_length: f32, original_colour: vec3f, n_numerical_integration_steps: i32) -> vec3f {
    var integral = integrate_atmoshpere_light(ray_origin.z, ray_direction.z, ray_length, n_numerical_integration_steps);
    let cos_sun = dot(ray_direction, vec3f(0.0, 0.0, 1.0));
    let phase_function = 0.75 * (1.0 + cos_sun * cos_sun);
    integral *= scattering_coefficients() * phase_function;
    let view_ray_optical_depth = an_optical_depth(ray_origin.z, ray_direction.z, ray_length);
    let transmittance = exp(-(view_ray_optical_depth) * scattering_coefficients());

    return integral + transmittance * original_colour;
}

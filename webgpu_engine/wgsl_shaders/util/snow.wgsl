/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Adam Celarek
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

#include "util/noise.wgsl"
#include "util/normals_util.wgsl"

//TODO find nice place to put
fn calculate_falloff(dist: f32, lower: f32, upper: f32) -> f32 { return clamp(1.0 - (dist - lower) / (upper - lower), 0.0, 1.0); }

fn calculate_band_falloff(val: f32, min: f32, max: f32, smoothf: f32) -> f32 {
    if (val < min) { return calculate_falloff(val, min + smoothf, min); }
    else if (val > max) { return calculate_falloff(val, max, max + smoothf); }
    else { return 1.0; }
}

fn overlay_snow(normal: vec3f, pos_ws: vec3f, snow_settings_angle: vec4f, snow_settings_alt: vec4f) -> vec4f {
    // Calculate steepness in deg where 90.0 = vertical (90°) and 0.0 = flat (0°)
    let steepness_deg = degrees(get_slope_angle(normal));

    let steepness_based_alpha = calculate_band_falloff(
                steepness_deg,
                snow_settings_angle.y,
                snow_settings_angle.z,
                snow_settings_angle.w);

    let lat_long_alt = world_to_lat_long_alt(pos_ws);
    let pos_noise_hf = noise(pos_ws / 70.0);
    let pos_noise_lf = noise(pos_ws / 500.0);
    let snow_border = snow_settings_alt.x
            + (snow_settings_alt.y * (2.0 * pos_noise_lf - 0.5))
            + (snow_settings_alt.y * (0.5 * (pos_noise_hf - 0.5)));
    let altitude_based_alpha = calculate_falloff(
                lat_long_alt.z,
                snow_border,
                snow_border - snow_settings_alt.z * pos_noise_lf) ;

    let snow_color = vec3f(1.0);
    return vec4f(snow_color, altitude_based_alpha * steepness_based_alpha);
}

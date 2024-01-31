/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

#define MAX_SNOW_ANGLE 30.0
#define SMOOTH_ANGLE_OFFSET 5.0

#define ORIGIN_SHIFT 20037508.3427892430765884
#define PI 3.1415926535897932384626433

highp float mod289(highp float x)   {   return x - floor(x * (1.0 / 289.0)) * 289.0;    }
highp vec4 mod289(highp vec4 x)     {   return x - floor(x * (1.0 / 289.0)) * 289.0;    }
highp vec4 perm(highp vec4 x)       {   return mod289(((x * 34.0) + 1.0) * x);          }

highp float noise(highp vec3 p){
    highp vec3 a = floor(p);
    highp vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);
    highp vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    highp vec4 k1 = perm(b.xyxy);
    highp vec4 k2 = perm(k1.xyxy + b.zzww);
    highp vec4 c = k2 + a.zzzz;
    highp vec4 k3 = perm(c);
    highp vec4 k4 = perm(c + 1.0);
    highp vec4 o1 = fract(k3 * (1.0 / 41.0));
    highp vec4 o2 = fract(k4 * (1.0 / 41.0));
    highp vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    highp vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);
    return o4.y * d.y + o4.x * (1.0 - d.y);
}

highp vec3 world_to_lat_long_alt(highp vec3 pos_ws) {
    highp float mercN = pos_ws.y * PI / ORIGIN_SHIFT;
    highp float latRad = 2.0 * (atan(exp(mercN)) - (PI / 4.0));
    highp float latitude = latRad * 180.0 / PI;
    highp float longitude = (pos_ws.x + ORIGIN_SHIFT) / (ORIGIN_SHIFT / 180.0) - 180.0;
    highp float altitude = pos_ws.z * cos(latitude * PI / 180.0);
    return vec3(latitude, longitude, altitude);
}

highp float calculate_band_falloff(highp float val, highp float min, highp float max, highp float smoothf) {
    if (val < min) return calculate_falloff(val, min + smoothf, min);
    else if (val > max) return calculate_falloff(val, max, max + smoothf);
    else return 1.0;
}

lowp vec4 overlay_snow(highp vec3 normal, highp vec3 pos_ws, highp float dist) {
    // Calculate steepness in deg where 90.0 = vertical (90°) and 0.0 = flat (0°)
    highp float steepness_deg = (1.0 - dot(normal, vec3(0.0,0.0,1.0))) * 90.0;

    highp float steepness_based_alpha = calculate_band_falloff(
                steepness_deg,
                conf.snow_settings_angle.y,
                conf.snow_settings_angle.z,
                conf.snow_settings_angle.w);

    highp vec3 lat_long_alt = world_to_lat_long_alt(pos_ws);
    highp float pos_noise_hf = noise(pos_ws / 70.0);
    highp float pos_noise_lf = noise(pos_ws / 500.0);
    highp float snow_border = conf.snow_settings_alt.x
            + (conf.snow_settings_alt.y * (2.0 * pos_noise_lf - 0.5))
            + (conf.snow_settings_alt.y * (0.5 * (pos_noise_hf - 0.5)));
    highp float altitude_based_alpha = calculate_falloff(
                lat_long_alt.z,
                snow_border,
                snow_border - conf.snow_settings_alt.z * pos_noise_lf) ;

    highp vec3 snow_color = vec3(1.0);
    return vec4(snow_color, steepness_based_alpha * altitude_based_alpha);
}

/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Gerald Kimmersdorfer
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

const lowp int steepness_bins = 10;
const lowp vec4 steepness_color_map[steepness_bins] = vec4[](
    vec4(254.0/255.0, 249.0/255.0, 249.0/255.0, 1.0),
    vec4(0.0/255.0, 115.0/255.0, 25.0/255.0, 1.0),
    vec4(51.0/255.0, 249.0/255.0, 49.0/255.0, 1.0),
    vec4(242.0/255.0, 228.0/255.0, 44.0/255.0, 1.0),
    vec4(255.0/255.0, 169.0/255.0, 45.0/255.0, 1.0),
    vec4(255.0/255.0, 48.0/255.0, 45.0/255.0, 1.0),
    vec4(255.0/255.0, 79.0/255.0, 249.0/255.0, 1.0),
    vec4(183.0/255.0, 69.0/255.0, 253.0/255.0, 1.0),
    vec4(135.0/255.0, 44.0/255.0, 253.0/255.0, 1.0),
    vec4(49.0/255.0, 49.0/255.0, 253.0/255.0, 1.0)
);

lowp vec4 overlay_steepness(highp vec3 normal, highp float dist) {
    highp float steepness_degrees = degrees(acos(dot(normal, vec3(0.0, 0.0, 1.0))));
    highp float alpha = clamp(1.0 - dist / 20000.0, 0.0, 1.0);
    lowp vec4 bin_color;
    if (steepness_degrees > 60.0){
        bin_color = steepness_color_map[9];
    } else if (steepness_degrees > 55.0){
        bin_color = steepness_color_map[8];
    } else if (steepness_degrees > 50.0){
        bin_color = steepness_color_map[7];
    } else if (steepness_degrees > 45.0){
        bin_color = steepness_color_map[6];
    } else if (steepness_degrees > 40.0){
        bin_color = steepness_color_map[5];
    } else if (steepness_degrees > 35.0){
        bin_color = steepness_color_map[4];
    } else if (steepness_degrees > 30.0){
        bin_color = steepness_color_map[3];
    } else if (steepness_degrees > 25.0){
        bin_color = steepness_color_map[2];
    } else if (steepness_degrees > 15.0){
        bin_color = steepness_color_map[1];
    }else {
        // less than 15°
        bin_color = steepness_color_map[0];
    }
    return vec4(bin_color.rgb, bin_color.a * alpha);
}

lowp vec3 gradient_color(highp float gradient) {
    lowp int bin_index = int(gradient * float(steepness_bins - 1));
    lowp vec4 bin_color = steepness_color_map[bin_index];
    return bin_color.rgb;
}

/*****************************************************************************
 * Alpine Terrain Renderer
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

const lowp int steepness_bins = 9;
const lowp vec4 steepness_color_map[steepness_bins] = vec4[](
    vec4(254.0/255.0, 249.0/255.0, 249.0/255.0, 1.0),
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
    highp float steepness = (1.0 - dot(normal, vec3(0.0,0.0,1.0)));
    highp float alpha = 1.0 - min((dist / 20000.0), 1.0);
    if (dist < 0.0) alpha = 0.0;
    lowp int bin_index = int(steepness * float(steepness_bins - 1) + 0.5);
    lowp vec4 bin_color = steepness_color_map[bin_index];
    return vec4(bin_color.rgb, bin_color.a * alpha);
}

lowp vec3 gradient_color(highp float gradient) {
    lowp int bin_index = int(gradient * float(steepness_bins - 1));
    lowp vec4 bin_color = steepness_color_map[bin_index];
    return bin_color.rgb;
}

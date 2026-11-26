/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2024 Jakob Maier
 * Copyright (C) 2024 Adam Celarek
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

#include "intersection.glsl"
#include "hashing.glsl"
#include "encoder.glsl"
#include "shared_config.glsl"
#include "turbo_colormap.glsl"
#include "overlay_steepness.glsl"

layout (location = 0) out lowp vec4 out_color;

flat in highp int vertex_id;
flat in int instance_id;

uniform highp sampler2D texin_track;
uniform highp sampler2D texin_position;
uniform highp vec3 camera_position;
uniform bool enable_intersection;
uniform int shading_method;
uniform highp mat4 view;
uniform highp mat4 proj;
uniform highp float max_speed;
uniform highp float max_vertical_speed;
uniform highp float width;
uniform int end_index;
uniform bool use_color_override;
uniform bool color_is_marker;
uniform highp vec3 color_override;
// the planned route line is meant to sit on the terrain. a tolerance band
// keeps it fully opaque through sub-metre sampling and lod-streaming noise;
// without it the capsule distance test flips between the opaque and the
// half-transparent below-terrain branch, which reads as a wiggle
uniform highp float terrain_tolerance;


uniform highp vec2 resolution;

highp vec3 visualize_normal(highp vec3 normal) {
    return (normal + vec3(1.0)) / vec3(2.0);
}

// steepness colouring for the route line: a 200-bin colour ramp sampled from
// the steepness legend palette (white below 15 degrees, legend colours at
// their bin centres, interpolated in between). the mix chain is generated
// (see the generator note in the repository history) - do not hand-edit the
// individual entries. written out as straight-line mixes with constant
// indices, since dynamic indexing of const arrays is not reliably supported
// on gles 3.0 compilers
lowp vec3 route_steepness_color(highp float steepness_degrees) {
    lowp vec3 colour = steepness_color_map[0].rgb; // white below 15 degrees
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 0.0000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 0.3250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 0.6500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 0.9750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 1.3000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 1.6250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 1.9500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 2.2750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 2.6000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 2.9250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 3.2500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 3.5750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 3.9000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 4.2250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 4.5500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 4.8750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 5.2000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 5.5250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 5.8500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 6.1750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 6.5000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 6.8250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 7.1500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 7.4750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 7.8000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 8.1250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 8.4500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 8.7750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 9.1000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 9.4250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 9.7500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 10.0750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 10.4000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 10.7250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 11.0500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 11.3750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 11.7000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 12.0250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 12.3500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 12.6750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 13.0000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 13.3250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 13.6500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 13.9750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 14.3000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.996, 0.976, 0.976), clamp((steepness_degrees - 14.6250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.990, 0.973, 0.967), clamp((steepness_degrees - 14.9500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.974, 0.964, 0.942), clamp((steepness_degrees - 15.2750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.958, 0.956, 0.917), clamp((steepness_degrees - 15.6000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.942, 0.947, 0.892), clamp((steepness_degrees - 15.9250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.925, 0.939, 0.868), clamp((steepness_degrees - 16.2500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.909, 0.930, 0.843), clamp((steepness_degrees - 16.5750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.893, 0.922, 0.818), clamp((steepness_degrees - 16.9000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.877, 0.913, 0.793), clamp((steepness_degrees - 17.2250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.851, 0.900, 0.764), clamp((steepness_degrees - 17.5500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.821, 0.884, 0.733), clamp((steepness_degrees - 17.8750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.791, 0.868, 0.703), clamp((steepness_degrees - 18.2000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.761, 0.852, 0.672), clamp((steepness_degrees - 18.5250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.731, 0.836, 0.642), clamp((steepness_degrees - 18.8500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.700, 0.820, 0.611), clamp((steepness_degrees - 19.1750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.670, 0.804, 0.581), clamp((steepness_degrees - 19.5000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.640, 0.789, 0.550), clamp((steepness_degrees - 19.8250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.613, 0.773, 0.525), clamp((steepness_degrees - 20.1500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.586, 0.758, 0.499), clamp((steepness_degrees - 20.4750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.559, 0.743, 0.474), clamp((steepness_degrees - 20.8000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.532, 0.728, 0.448), clamp((steepness_degrees - 21.1250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.505, 0.713, 0.423), clamp((steepness_degrees - 21.4500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.478, 0.697, 0.397), clamp((steepness_degrees - 21.7750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.451, 0.682, 0.372), clamp((steepness_degrees - 22.1000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.424, 0.667, 0.349), clamp((steepness_degrees - 22.4250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.400, 0.651, 0.332), clamp((steepness_degrees - 22.7500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.376, 0.635, 0.316), clamp((steepness_degrees - 23.0750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.351, 0.619, 0.299), clamp((steepness_degrees - 23.4000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.327, 0.603, 0.283), clamp((steepness_degrees - 23.7250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.302, 0.587, 0.267), clamp((steepness_degrees - 24.0500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.278, 0.572, 0.250), clamp((steepness_degrees - 24.3750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.253, 0.556, 0.234), clamp((steepness_degrees - 24.7000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.225, 0.542, 0.217), clamp((steepness_degrees - 25.0250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.193, 0.529, 0.201), clamp((steepness_degrees - 25.3500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.162, 0.516, 0.184), clamp((steepness_degrees - 25.6750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.130, 0.503, 0.167), clamp((steepness_degrees - 26.0000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.098, 0.491, 0.150), clamp((steepness_degrees - 26.3250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.067, 0.478, 0.133), clamp((steepness_degrees - 26.6500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.035, 0.465, 0.117), clamp((steepness_degrees - 26.9750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.004, 0.452, 0.100), clamp((steepness_degrees - 27.3000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.023, 0.511, 0.109), clamp((steepness_degrees - 27.6250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.049, 0.580, 0.121), clamp((steepness_degrees - 27.9500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.075, 0.648, 0.133), clamp((steepness_degrees - 28.2750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.101, 0.716, 0.145), clamp((steepness_degrees - 28.6000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.127, 0.784, 0.158), clamp((steepness_degrees - 28.9250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.153, 0.853, 0.170), clamp((steepness_degrees - 29.2500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.179, 0.921, 0.182), clamp((steepness_degrees - 29.5750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.209, 0.975, 0.192), clamp((steepness_degrees - 29.9000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.258, 0.969, 0.190), clamp((steepness_degrees - 30.2250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.306, 0.964, 0.189), clamp((steepness_degrees - 30.5500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.355, 0.958, 0.187), clamp((steepness_degrees - 30.8750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.403, 0.953, 0.185), clamp((steepness_degrees - 31.2000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.452, 0.947, 0.184), clamp((steepness_degrees - 31.5250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.500, 0.941, 0.182), clamp((steepness_degrees - 31.8500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.549, 0.936, 0.181), clamp((steepness_degrees - 32.1750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.597, 0.930, 0.180), clamp((steepness_degrees - 32.5000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.646, 0.925, 0.179), clamp((steepness_degrees - 32.8250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.695, 0.920, 0.178), clamp((steepness_degrees - 33.1500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.744, 0.915, 0.177), clamp((steepness_degrees - 33.4750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.793, 0.910, 0.176), clamp((steepness_degrees - 33.8000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.842, 0.905, 0.175), clamp((steepness_degrees - 34.1250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.891, 0.900, 0.174), clamp((steepness_degrees - 34.4500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.940, 0.895, 0.173), clamp((steepness_degrees - 34.7750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.954, 0.870, 0.173), clamp((steepness_degrees - 35.1000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.961, 0.840, 0.174), clamp((steepness_degrees - 35.4250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.968, 0.810, 0.174), clamp((steepness_degrees - 35.7500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.974, 0.780, 0.174), clamp((steepness_degrees - 36.0750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.981, 0.750, 0.175), clamp((steepness_degrees - 36.4000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.988, 0.720, 0.175), clamp((steepness_degrees - 36.7250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.994, 0.690, 0.176), clamp((steepness_degrees - 37.0500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.659, 0.176), clamp((steepness_degrees - 37.3750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.628, 0.176), clamp((steepness_degrees - 37.7000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.597, 0.176), clamp((steepness_degrees - 38.0250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.566, 0.176), clamp((steepness_degrees - 38.3500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.535, 0.176), clamp((steepness_degrees - 38.6750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.504, 0.176), clamp((steepness_degrees - 39.0000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.473, 0.176), clamp((steepness_degrees - 39.3250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.442, 0.176), clamp((steepness_degrees - 39.6500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.411, 0.176), clamp((steepness_degrees - 39.9750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.380, 0.176), clamp((steepness_degrees - 40.3000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.350, 0.176), clamp((steepness_degrees - 40.6250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.319, 0.176), clamp((steepness_degrees - 40.9500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.288, 0.176), clamp((steepness_degrees - 41.2750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.258, 0.176), clamp((steepness_degrees - 41.6000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.227, 0.176), clamp((steepness_degrees - 41.9250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.196, 0.176), clamp((steepness_degrees - 42.2500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.194, 0.214), clamp((steepness_degrees - 42.5750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.201, 0.266), clamp((steepness_degrees - 42.9000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.209, 0.318), clamp((steepness_degrees - 43.2250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.217, 0.370), clamp((steepness_degrees - 43.5500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.224, 0.422), clamp((steepness_degrees - 43.8750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.232, 0.474), clamp((steepness_degrees - 44.2000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.240, 0.526), clamp((steepness_degrees - 44.5250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.247, 0.578), clamp((steepness_degrees - 44.8500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.256, 0.630), clamp((steepness_degrees - 45.1750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.264, 0.682), clamp((steepness_degrees - 45.5000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.272, 0.734), clamp((steepness_degrees - 45.8250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.280, 0.786), clamp((steepness_degrees - 46.1500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.288, 0.838), clamp((steepness_degrees - 46.4750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.296, 0.890), clamp((steepness_degrees - 46.8000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(1.000, 0.305, 0.942), clamp((steepness_degrees - 47.1250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.994, 0.309, 0.976), clamp((steepness_degrees - 47.4500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.975, 0.306, 0.977), clamp((steepness_degrees - 47.7750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.957, 0.304, 0.978), clamp((steepness_degrees - 48.1000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.939, 0.301, 0.979), clamp((steepness_degrees - 48.4250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.920, 0.299, 0.981), clamp((steepness_degrees - 48.7500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.902, 0.296, 0.982), clamp((steepness_degrees - 49.0750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.884, 0.293, 0.983), clamp((steepness_degrees - 49.4000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.865, 0.291, 0.984), clamp((steepness_degrees - 49.7250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.847, 0.288, 0.985), clamp((steepness_degrees - 50.0500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.829, 0.286, 0.986), clamp((steepness_degrees - 50.3750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.810, 0.283, 0.987), clamp((steepness_degrees - 50.7000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.792, 0.281, 0.988), clamp((steepness_degrees - 51.0250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.774, 0.279, 0.989), clamp((steepness_degrees - 51.3500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.755, 0.276, 0.990), clamp((steepness_degrees - 51.6750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.737, 0.274, 0.991), clamp((steepness_degrees - 52.0000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.719, 0.271, 0.992), clamp((steepness_degrees - 52.3250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.706, 0.265, 0.992), clamp((steepness_degrees - 52.6500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.694, 0.258, 0.992), clamp((steepness_degrees - 52.9750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.682, 0.251, 0.992), clamp((steepness_degrees - 53.3000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.670, 0.245, 0.992), clamp((steepness_degrees - 53.6250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.657, 0.238, 0.992), clamp((steepness_degrees - 53.9500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.645, 0.231, 0.992), clamp((steepness_degrees - 54.2750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.633, 0.225, 0.992), clamp((steepness_degrees - 54.6000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.621, 0.218, 0.992), clamp((steepness_degrees - 54.9250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.608, 0.212, 0.992), clamp((steepness_degrees - 55.2500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.596, 0.206, 0.992), clamp((steepness_degrees - 55.5750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.584, 0.200, 0.992), clamp((steepness_degrees - 55.9000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.571, 0.194, 0.992), clamp((steepness_degrees - 56.2250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.559, 0.188, 0.992), clamp((steepness_degrees - 56.5500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.547, 0.182, 0.992), clamp((steepness_degrees - 56.8750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.534, 0.176, 0.992), clamp((steepness_degrees - 57.2000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.516, 0.174, 0.992), clamp((steepness_degrees - 57.5250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.495, 0.174, 0.992), clamp((steepness_degrees - 57.8500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.473, 0.175, 0.992), clamp((steepness_degrees - 58.1750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.451, 0.176, 0.992), clamp((steepness_degrees - 58.5000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.429, 0.177, 0.992), clamp((steepness_degrees - 58.8250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.407, 0.178, 0.992), clamp((steepness_degrees - 59.1500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.385, 0.179, 0.992), clamp((steepness_degrees - 59.4750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.364, 0.180, 0.992), clamp((steepness_degrees - 59.8000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.351, 0.181, 0.992), clamp((steepness_degrees - 60.1250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.340, 0.181, 0.992), clamp((steepness_degrees - 60.4500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.329, 0.182, 0.992), clamp((steepness_degrees - 60.7750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.318, 0.183, 0.992), clamp((steepness_degrees - 61.1000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.307, 0.184, 0.992), clamp((steepness_degrees - 61.4250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.296, 0.185, 0.992), clamp((steepness_degrees - 61.7500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.285, 0.185, 0.992), clamp((steepness_degrees - 62.0750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.274, 0.186, 0.992), clamp((steepness_degrees - 62.4000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.263, 0.187, 0.992), clamp((steepness_degrees - 62.7250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.252, 0.188, 0.992), clamp((steepness_degrees - 63.0500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.241, 0.188, 0.992), clamp((steepness_degrees - 63.3750) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.230, 0.189, 0.992), clamp((steepness_degrees - 63.7000) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.219, 0.190, 0.992), clamp((steepness_degrees - 64.0250) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.208, 0.191, 0.992), clamp((steepness_degrees - 64.3500) / 0.3250, 0.0, 1.0));
    colour = mix(colour, vec3(0.197, 0.192, 0.992), clamp((steepness_degrees - 64.6750) / 0.3250, 0.0, 1.0));
    return colour;
}

// https://sibaku.github.io/computer-graphics/2017/01/10/Camera-Ray-Generation.html
highp vec3 camera_ray(highp vec2 px, highp mat4 inverse_proj, highp mat4 inverse_view)
{
    highp vec2 pxNDS =  2. * px - 1.;
    highp vec3 pointNDS = vec3(pxNDS, -1.);
    highp vec4 pointNDSH = vec4(pointNDS, 1.0);

    highp vec4 eye_dir = inverse_proj * pointNDSH;
    eye_dir.w = 0.;

    highp vec3 world_dir = (inverse_view * eye_dir).xyz;

    return normalize(world_dir);
}

highp vec3 calc_blinn_phong_contribution(highp vec3 toLight, highp vec3 toEye, highp vec3 normal, highp vec3 diffFactor, highp vec3 specFactor, highp float specShininess)
{
    highp float nDotL = max(0.0, dot(normal, toLight)); // lambertian coefficient
    highp vec3 h = normalize(toLight + toEye);
    highp float nDotH = max(0.0, dot(normal, h));
    highp float specPower = pow(nDotH, specShininess);
    highp vec3 diffuse = diffFactor * nDotL; // component-wise product
    highp vec3 specular = specFactor * specPower;
    return diffuse + specular;
}

highp vec3 phong_lighting(highp vec3 albedo, highp vec3 normal, highp vec3 eyePos, highp vec3 fragPos) {
    highp vec4 dirLight = conf.sun_light;
    highp vec4 ambLight  =  conf.amb_light;
    highp vec3 sun_light_dir = conf.sun_light_dir.xyz;
    highp vec4 material = conf.material_light_response;

    highp vec3 dirColor = dirLight.rgb * dirLight.a;
    highp vec3 ambColor = ambLight.rgb * ambLight.a;

    highp vec3 ambient = material.r * albedo;
    highp vec3 diff = material.g * albedo;
    highp vec3 spec = vec3(material.b);
    highp float shini = material.a;

    highp vec3 toLightDirWS = -normalize(conf.sun_light_dir.xyz);
    highp vec3 toEyeNrmWS = normalize(eyePos - fragPos);

    highp vec3 diffAndSpecIllumination = dirColor * calc_blinn_phong_contribution(toLightDirWS, toEyeNrmWS, normal, diff, spec, shini);

    return (ambient * ambColor) + diffAndSpecIllumination;
}

void main() {

    if (!enable_intersection) {
        // only for debugging
        out_color = vec4(color_from_id_hash(uint(vertex_id)), 1);

    } else {

        highp vec2 texcoords = gl_FragCoord.xy / resolution.xy;

        highp vec3 sun_light_dir = conf.sun_light_dir.xyz;

        // track vertex position
        int id = vertex_id;

        highp vec4 p1 = texelFetch(texin_track, ivec2(int(id + 0), 0), 0);
        highp vec4 p2 = texelFetch(texin_track, ivec2(int(id + 1), 0), 0);

        highp vec3 x0 = texelFetch(texin_track, ivec2(int(id - 1), 0), 0).xyz - camera_position;
        highp vec3 x1 = p1.xyz - camera_position;
        highp vec3 x2 = p2.xyz - camera_position;
        highp vec3 x3 = texelFetch(texin_track, ivec2(int(id + 2), 0), 0).xyz - camera_position;

        highp float delta_time = p2.w;

        highp vec4 pos_dist = texture(texin_position, texcoords);

        // terrain position
        highp vec3 terrain_pos = pos_dist.xyz;

        // distance from camera to terrain, negative if sky
        highp float dist = pos_dist.w;

        Ray ray;

        if (dist < 0.) {
            // ray does not hit terrain, it hits sky

            highp vec3 dir = camera_ray(texcoords, inverse(proj), inverse(view));
            ray = Ray(vec3(0, 0, 0), dir);
            dist = INF;

        } else {
            // ray does hit terrain
            ray = Ray(vec3(0, 0, 0), normalize(terrain_pos / abs(dist)));
        }

        Capsule c = Capsule(x1, x2, width);

        highp float t = intersect_capsule_2(ray, c);

        if ((0.0 < t && t < INF)) {

            highp vec3 point = ray.origin + ray.direction * t;

            highp vec3 normal = capsule_normal_2(point, c);

            Plane clipping_plane_1, clipping_plane_2;

            highp vec3 n0 = -normalize(x2 - x0);
            clipping_plane_1.normal = n0;
            clipping_plane_1.distance = dot(x1, n0);

            highp vec3 n1 = -normalize(x3 - x1);
            clipping_plane_2.normal = n1;
            clipping_plane_2.distance = dot(x2, n1);

            if (
                (0.5 >= signed_distance(point, clipping_plane_1) || (vertex_id == 0)) &&
                (-0.5 <= signed_distance(point, clipping_plane_2) || (vertex_id == end_index - 1))
            )
            {

                highp vec3 color;

                highp vec3 RED = vec3(1., 0., 0.);
                highp vec3 BLUE = vec3(0., 0., 1.);

                highp vec3 A = x1;
                highp vec3 AP = point - x1;
                highp vec3 AB = x2 - x1;

                // where point is along the capsule major axis
                highp float f = dot(AP, AB) / dot(AB, AB);

                if (shading_method == 0) { // default
                    color = vec3(1,0,0);

                } else if (shading_method == 1) { // normal

                    color = visualize_normal(normal);

                } else if (shading_method == 2) { // speed

                    highp float speed_0 = length(x0 - x1) / p1.w;
                    highp float speed_1 = length(x1 - x2) / p2.w;

                    highp float t_0 = clamp(speed_0 / max_speed, 0., 1.);
                    highp float t_1 = clamp(speed_1 / max_speed, 0., 1.);

                    color = mix(TurboColormap(t_0), TurboColormap(t_1), f);

                } else if (shading_method == 3) {  // steepness
                    highp vec3 d1 = normalize(x0 - x1);
                    highp vec3 d2 = normalize(x1 - x2);

                    highp vec3 up = vec3(0.0, 0.0, 1.0);

                    color = mix( TurboColormap(abs(dot(d1, up))), TurboColormap(abs(dot(d2, up))), f);

                } else if (shading_method == 4) {  // vertical speed

                    highp float speed_0 = abs(x0.z - x1.z) / p1.w;
                    highp float speed_1 = abs(x1.z - x2.z) / p2.w;

                    highp float t_0 = clamp(speed_0 / max_vertical_speed, 0., 1.);
                    highp float t_1 = clamp(speed_1 / max_vertical_speed, 0., 1.);

                    color = mix(TurboColormap(t_0), TurboColormap(t_1), f);

                } else {
                    color = vec3(0);
                }

                if (use_color_override) {
                    if (color_is_marker || conf.overlay_mode == 101u) {
                        // markers and (while the steepness terrain overlay is
                        // active, which would make steepness colouring
                        // invisible) the line use the flat colour
                        color = color_override;
                    } else {
                        // colour the route line by the steepness of each
                        // segment (angle between the segment direction and the
                        // horizontal plane), with the steepness overlay colour
                        // map. the slope is averaged over this segment and its
                        // two neighbours, so the colour changes gradually.
                        // at the line ends x0/x3 fetch out of range; guard
                        // against it by comparing against this segment's length
                        highp float l12 = length(x2 - x1);
                        highp float l01 = length(x1 - x0);
                        highp float l23 = length(x3 - x2);
                        highp float dz12 = abs(x2.z - x1.z);
                        highp float dz01 = abs(x1.z - x0.z);
                        highp float dz23 = abs(x3.z - x2.z);
                        if (l01 > 4.0 * l12 || l01 <= 0.0) {
                            l01 = l12;
                            dz01 = dz12;
                        }
                        if (l23 > 4.0 * l12 || l23 <= 0.0) {
                            l23 = l12;
                            dz23 = dz12;
                        }
                        highp float slope = (dz01 + dz12 + dz23) / (l01 + l12 + l23);
                        highp float slope_degrees = degrees(asin(clamp(slope, 0.0, 1.0)));
                        color = route_steepness_color(slope_degrees);
                    }
                }

                color = phong_lighting(color, normal, vec3(0, 0, 0), point);

                if (t < dist + terrain_tolerance) {
                    // geometry is above terrain (or within the tolerance band)
                    out_color = vec4(color, 1.0);

                } else {

                    // track has constant alpha upto min_below, then it slowly falls off
                    highp float distance_below = t - dist;
                    highp float min_below = 100.0;
                    highp float max_below = 2000.0;

                    if (distance_below < min_below) {
                        out_color = vec4(color, 0.5);
                    } else {
                        highp float t = (distance_below - min_below) / (max_below - min_below);
                        out_color = vec4(color, 0.5 * (1.0 - t));
                    }
                }
            } else {
                discard; // clipping
            }
        } else {
            discard; // no intersection
        }


    }
}

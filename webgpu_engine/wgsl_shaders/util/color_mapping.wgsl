/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

// value in [0,1], represents angle from horizontal to vertical
// color map from https://www.bergfex.com/
// values not in [0,1] are mapped to black
fn color_mapping_bergfex(value: f32) -> vec3f {
    if (value < 0.0 || value > 1.0) {
        return vec3f(0);
    }

    let deg = u32(value * 90);

    const bins = array<u32, 5>(29, 34, 39, 44, 90);
    const colors = array<vec3f, 5>(vec3f(1, 1, 1), vec3f(255.0 / 255.0, 219.0 / 255.0, 17.0 / 255.0), vec3f(255.0 / 255.0, 117.0 / 255.0, 6.0 / 255.0), vec3f(213.0 / 255.0, 0.0 / 255.0, 0.0 / 255.0), vec3f(100.0 / 255.0, 0.0 / 255.0, 213.0 / 255.0),);

    var index = 0;
    while (deg > bins[index]) {
        index++;
    }
    return colors[index];
}

// value in [0,1], represents angle from horizontal to vertical
// color map from https://www.openslopemap.org/karte/
// values not in [0,1] are mapped to black
fn color_mapping_openslopemap(value: f32) -> vec3f {
    if (value < 0.0 || value > 1.0) {
        return vec3f(0);
    }

    let deg = u32(value * 90);

    const bins = array<u32, 9>(9, 29, 34, 39, 42, 45, 49, 54, 90);
    const colors = array<vec3f, 9>(vec3f(254.0 / 255.0, 249.0 / 255.0, 249.0 / 255.0), vec3f(51.0 / 255.0, 249.0 / 255.0, 49.0 / 255.0), vec3f(242.0 / 255.0, 228.0 / 255.0, 44.0 / 255.0), vec3f(255.0 / 255.0, 169.0 / 255.0, 45.0 / 255.0), vec3f(255.0 / 255.0, 48.0 / 255.0, 45.0 / 255.0), vec3f(255.0 / 255.0, 79.0 / 255.0, 249.0 / 255.0), vec3f(183.0 / 255.0, 69.0 / 255.0, 253.0 / 255.0), vec3f(135.0 / 255.0, 44.0 / 255.0, 253.0 / 255.0), vec3f(49.0 / 255.0, 49.0 / 255.0, 253.0 / 255.0),);

    var index = 0;
    while (deg > bins[index]) {
        index++;
    }
    return colors[index];
}

// value in [0,1], just use red color channel
fn color_mapping_red(value: f32) -> vec3f {
    return vec3f(value, 0, 0);
}

// the color mapping used in the flowpy paper
fn color_mapping_flowpy(value: f32, min: f32, max: f32, blend: bool) -> vec4f {
    const bin_count: u32 = 20u;

    //normalize value to [0,1]
    let normalized = (value - min) / (max - min);

    //define the color bins
    //in hex: 404043, 454455, 504a6b, 5e4c83, 6f4b96, 7f4f9d, 9055a1, 9f5ba1, b061a1, c0669d, d16b97, e07391, ee7e89, f78e85, fca187, feb38f, fec79c, fed9ab, feecbc, fdfecf
    const colors = array<vec4f, bin_count>(vec4f(0x40 / 255.0, 0x40 / 255.0, 0x43 / 255.0, 1.0), vec4f(0x45 / 255.0, 0x44 / 255.0, 0x55 / 255.0, 1.0), vec4f(0x50 / 255.0, 0x4a / 255.0, 0x6b / 255.0, 1.0), vec4f(0x5e / 255.0, 0x4c / 255.0, 0x83 / 255.0, 1.0), vec4f(0x6f / 255.0, 0x4b / 255.0, 0x96 / 255.0, 1.0), vec4f(0x7f / 255.0, 0x4f / 255.0, 0x9d / 255.0, 1.0), vec4f(0x90 / 255.0, 0x55 / 255.0, 0xa1 / 255.0, 1.0), vec4f(0x9f / 255.0, 0x5b / 255.0, 0xa1 / 255.0, 1.0), vec4f(0xb0 / 255.0, 0x61 / 255.0, 0xa1 / 255.0, 1.0), vec4f(0xc0 / 255.0, 0x66 / 255.0, 0x9d / 255.0, 1.0), vec4f(0xd1 / 255.0, 0x6b / 255.0, 0x97 / 255.0, 1.0), vec4f(0xe0 / 255.0, 0x73 / 255.0, 0x91 / 255.0, 1.0), vec4f(0xee / 255.0, 0x7e / 255.0, 0x89 / 255.0, 1.0), vec4f(0xf7 / 255.0, 0x8e / 255.0, 0x85 / 255.0, 1.0), vec4f(0xfc / 255.0, 0xa1 / 255.0, 0x87 / 255.0, 1.0), vec4f(0xfe / 255.0, 0xb3 / 255.0, 0x8f / 255.0, 1.0), vec4f(0xfe / 255.0, 0xc7 / 255.0, 0x9c / 255.0, 1.0), vec4f(0xfe / 255.0, 0xd9 / 255.0, 0xab / 255.0, 1.0), vec4f(0xfe / 255.0, 0xec / 255.0, 0xbc / 255.0, 1.0), vec4f(0xfd / 255.0, 0xfe / 255.0, 0xcf / 255.0, 1.0));

    // Calculate bin index and interpolation factor
    let pos = normalized * (f32(bin_count) - 1.0);
    let index = u32(pos);
    let factor = pos - f32(index);

    if (!blend || index >= bin_count - 1) {
        return colors[index];
    }
    else {
        // Blend between current and next color
        let color1 = colors[index];
        let color2 = colors[index + 1];
        return mix(color1, color2, factor);
    }
}
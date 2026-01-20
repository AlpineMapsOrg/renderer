/*****************************************************************************
* Alpine Renderer
* Copyright (C) 2022 Adam Celarek
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

// Implementation of the octahedral normal encoding based on the paper
// "A Survey of Efficient Representations for Independent Unit Vector" by Cigolle et al.
// see https://jcgt.org/published/0003/02/01/ listings 1 and 2

// Custom implementation of the sign function for a vec2f which in contrast to the standard sign function
// returns 1.0 for 0.0 as input.
// - v: A vec2f representing the input vector.
// - Returns: A vec2f containing the componentwise sign of the input vector.
fn signNotZero(v: vec2f) -> vec2f {
    return sign(v) + vec2f(v == vec2f(0.0));
}

// Converts a normalized 3D vector into 2D octahedral coordinates.
// - n: A normalized vec3f representing the input normal vector.
// - Returns: A vec2f representing the normal vector encoded in the range [-1, 1] in an octahedral projection.
// - Requirement: n must be normalized.
fn v3f32_to_oct(n: vec3<f32>) -> vec2<f32> {
    var en: vec2<f32> = n.xy * (1.0 / (abs(n.x) + abs(n.y) + abs(n.z)));
    if (n.z <= 0.0) {
        en = (1.0 - abs(en.yx)) * signNotZero(en);
    }
    return en;
}

// Converts 2D octahedral coordinates back to a normalized 3D vector.
// - en: A vec2f representing the octahedral projection coordinates.
// - Returns: A normalized vec3f containing the decoded normal vector.
fn oct_to_v3f32(en: vec2<f32>) -> vec3<f32> {
    var n: vec3<f32> = vec3f(en.xy, 1.0 - abs(en.x) - abs(en.y));
    if (n.z < 0.0) {
        n = vec3f((1.0 - abs(n.yx)) * signNotZero(n.xy), n.z);
    }
    return normalize(n);
}

// Encodes a 2D vector into a 2D unsigned integer vector using a 16-bit range.
// - v: A vec2f representing the input vector in the range [0, 1].
// - Returns: A vec2u32 representing the encoded values with each component scaled to the range [0, 65535].
// - Requirement: v must be in the range [0, 1].
fn v2f32_to_v2u16(v: vec2<f32>) -> vec2<u32> {
    return vec2<u32>(u32(v.x * 65535.0), u32(v.y * 65535.0));
}

// Decodes a 2D unsigned integer vector into a normalized 2D vector in the range [0, 1].
// - e: A vec2u32 representing the encoded values with each component in the range [0, 65535].
// - Returns: A vec2f representing the decoded values in the range [0, 1].
fn v2u16_to_v2f32(e: vec2<u32>) -> vec2<f32> {
    return vec2<f32>(f32(e.x) / 65535.0, f32(e.y) / 65535.0);
}

// Encodes a normalized 3D vector into a 2D unsigned integer vector using octahedral projection.
// - n: A normalized vec3f representing the input normal vector.
// - Returns: A vec2u32 representing the encoded values using octahedral projection with each component in the range [0, 65535].
// - Requirement: n must be normalized.
fn octNormalEncode2u16(n: vec3<f32>) -> vec2<u32> {
    return v2f32_to_v2u16(fma(v3f32_to_oct(n), vec2f(0.5), vec2f(0.5)));
}

// Decodes a 2D unsigned integer vector into a normalized 3D vector using octahedral projection.
// - e: A vec2u32 representing the encoded octahedral projection coordinates with each component in the range [0, 65535].
// - Returns: A vec3f representing the normalized decoded normal vector.
fn octNormalDecode2u16(e: vec2<u32>) -> vec3<f32> {
    return oct_to_v3f32(fma(v2u16_to_v2f32(e), vec2f(2.0), vec2f(- 1.0)));
}

// Common range constants
const U32_ENCODING_RANGE_NORM: vec2f = vec2f(0.0, 1.0);
const U32_ENCODING_RANGE_VALIDATION: vec2f = vec2f(- 10000.0, 10000.0);

// Encodes a float value into a 32-bit unsigned integer using a range
// - value: Input value to encode
// - range: vec2f containing (min, max) of input range (default: [0,1])
// - Returns: u32 in [0, 4294967295]
fn range_to_u32(value: f32, range: vec2f) -> u32 {
    let normalized = clamp((value - range.x) / (range.y - range.x), 0.0, 1.0);
    return u32(normalized * f32(0xFFFFFFFFu));
}

// Decodes a u32 back to original range
// - encoded: u32 to decode
// - range: vec2f containing (min, max) of output range (default: [0,1])
// - Returns: Reconstructed float value
fn u32_to_range(encoded: u32, range: vec2f) -> f32 {
    let normalized = f32(encoded) / f32(0xFFFFFFFFu);
    return mix(range.x, range.y, normalized);
}
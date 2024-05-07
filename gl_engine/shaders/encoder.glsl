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

// ===== DEPTH ENCODE =====
// NOTE: Only positions up to e^13 = 442413 are in between [0,1]
// Thats not perfect, but I'll leave it as it is...
// Better would be to normalize it in between near and farplane
// or just use the cs Depth immediately.
highp float fakeNormalizeWSDepth(highp float depth) {
    return log(depth) / 13.0;
}
lowp vec2 depthWSEncode2n8(highp float depth) {
    highp float value = fakeNormalizeWSDepth(depth);
    mediump uint scaled = uint(value * 65535.f + 0.5f);
    mediump uint r = scaled >> 8u;
    mediump uint b = scaled & 255u;
    return vec2(float(r) / 255.f, float(b) / 255.f);
}

// ===== BYTE ENCODING ====

// Encodes a 2D vector into a 2D unsigned integer vector using a 16-bit range.
// - v: A highp vec2 representing the input vector in the range [0, 1].
// - Returns: A highp uvec2 representing the encoded values with each component scaled to the range [0, 65535].
// - Requirement: v must be in the range [0, 1].
highp uvec2 v2f32_to_v2u16(highp vec2 v) {
    return uvec2(uint(v.x * 65535.0), uint(v.y * 65535.0));
}

// Decodes a 2D unsigned integer vector into a normalized 2D vector in the range [0, 1].
// - e: A highp uvec2 representing the encoded values with each component in the range [0, 65535].
// - Returns: A highp vec2 representing the decoded values in the range [0, 1].
highp vec2 v2u16_to_v2f32(highp uvec2 e) {
    return vec2(float(e.x) / 65535.0, float(e.y) / 65535.0);
}

// ===== OCTAHEDRON MAPPING FOR NORMALS =====
// Implementation of the octahedral normal encoding based on the paper
// "A Survey of Efficient Representations for Independent Unit Vector" by Cigolle et al.
// see https://jcgt.org/published/0003/02/01/ listings 1 and 2

// Note: Normal sign in OpenGL returns 0 if value is zero.
highp vec2 signNotZero(highp vec2 v) {
    return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

// Converts a normalized 3D vector into 2D octahedral coordinates.
// - n: A normalized highp vec3 representing the input normal vector.
// - Returns: A highp vec2 representing the normal vector encoded in the range [-1, 1] in an octahedral projection.
// - Requirement: n must be normalized.
highp vec2 v3f32_to_oct(highp vec3 n) {
    highp vec2 ne = n.xy * (1.0 / (abs(n.x) + abs(n.y) + abs(n.z)));
    return (n.z <= 0.0) ? ((1.0 - abs(ne.yx)) * signNotZero(ne)) : ne;
}

// Converts 2D octahedral coordinates back to a normalized 3D vector.
// - en: A highp vec2 representing the octahedral projection coordinates.
// - Returns: A highp vec3 containing the decoded normal vector.
highp vec3 oct_to_v3f32(highp vec2 en) {
    highp vec3 v = vec3(en.xy, 1.0 - abs(en.x) - abs(en.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
    }
    return normalize(v);
}

// Encodes a normalized 3D vector into a 2D unsigned integer vector using octahedral projection.
// - n: A normalized highp vec3 representing the input normal vector.
// - Returns: A highp uvec2 representing the encoded values using octahedral projection with each component in the range [0, 65535].
// - Requirement: n must be normalized.
highp uvec2 octNormalEncode2u16(highp vec3 n) {
    return v2f32_to_v2u16(v3f32_to_oct(n) * vec2(0.5) + vec2(0.5));
}

// Decodes a 2D unsigned integer vector into a normalized 3D vector using octahedral projection.
// - e: A highp uvec2 representing the encoded octahedral projection coordinates with each component in the range [0, 65535].
// - Returns: A highp vec3 representing the normalized decoded normal vector.
highp vec3 octNormalDecode2u16(highp uvec2 e) {
    return oct_to_v3f32(v2u16_to_v2f32(e) * vec2(2.0) + vec2(-1.0));
}

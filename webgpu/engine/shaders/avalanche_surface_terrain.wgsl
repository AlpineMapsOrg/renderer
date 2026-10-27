/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

// The solver's terrain for the flow-surface shaders; include after avalanche_surface_common
// and declare `height_texture` (texture_2d<f32>).

// Catmull-Rom weights of the four texels around a sample, t the position between the middle two.
fn catmull_rom(t: f32) -> vec4f {
    let t2 = t * t;
    let t3 = t2 * t;
    return vec4f(-0.5 * t3 + t2 - 0.5 * t, 1.5 * t3 - 2.5 * t2 + 1.0, -1.5 * t3 + 2.0 * t2 + 0.5 * t, 0.5 * t3 - 0.5 * t2);
}

// Terrain altitude [m] at a point in metres. The texture is R32Float, so not filterable. Catmull-
// Rom rather than the solver's bilinear sample_height_uv (mpm_common.wgsl): it passes through the
// same texel heights, but its slope is continuous, so the surfaces draped over it (a thin cover
// layer above all) are not lit in blocks of the texture's texels. Needs `height_texture`.
fn terrain_altitude(metres: vec2f) -> f32 {
    let uv = vec2f(metres.x / surface.region_size.x, 1.0 - metres.y / surface.region_size.y);
    let coord = uv * vec2f(surface.height_dim) - vec2f(0.5);
    let base = floor(coord);
    let wx = catmull_rom(coord.x - base.x);
    let wy = catmull_rom(coord.y - base.y);
    let b = vec2i(base) - vec2i(1);
    let last = vec2i(surface.height_dim) - vec2i(1);
    var altitude = 0.0;
    for (var j = 0; j < 4; j++) {
        var row = 0.0;
        for (var i = 0; i < 4; i++) {
            row += wx[i] * textureLoad(height_texture, vec2u(clamp(b + vec2i(i, j), vec2i(0), last)), 0).r;
        }
        altitude += wy[j] * row;
    }
    return altitude;
}

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

// Terrain altitude [m] at a point in metres, bilinear like the solver's sample_height_uv
// (mpm_common.wgsl): the texture is R32Float, so not filterable. Needs `height_texture`.
fn terrain_altitude(metres: vec2f) -> f32 {
    let uv = vec2f(metres.x / surface.region_size.x, 1.0 - metres.y / surface.region_size.y);
    let coord = uv * vec2f(surface.height_dim) - vec2f(0.5);
    let base = floor(coord);
    let f = coord - base;
    let b = vec2i(base);
    let last = vec2i(surface.height_dim) - vec2i(1);
    let h00 = textureLoad(height_texture, vec2u(clamp(b, vec2i(0), last)), 0).r;
    let h10 = textureLoad(height_texture, vec2u(clamp(b + vec2i(1, 0), vec2i(0), last)), 0).r;
    let h01 = textureLoad(height_texture, vec2u(clamp(b + vec2i(0, 1), vec2i(0), last)), 0).r;
    let h11 = textureLoad(height_texture, vec2u(clamp(b + vec2i(1, 1), vec2i(0), last)), 0).r;
    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}

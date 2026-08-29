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

///use mpm_common

// Converts the accumulated density raster into the node's RGBA output texture.
// Density is mapped logarithmically because particle counts per texel span orders of
// magnitude between the thin flowing front and the deposited body.

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    if id.x >= settings.raster_dim.x || id.y >= settings.raster_dim.y {
        return;
    }

    let raster_index = id.y * settings.raster_dim.x + id.x;
    let count = atomicLoad(&density_raster[raster_index]);

    if count == 0u {
        textureStore(output_texture, id.xy, vec4f(0.0));
        return;
    }

    // log2(1 + n) normalised against a full texel of ~64 particles.
    let density = clamp(log2(1.0 + f32(count)) / 6.0, 0.0, 1.0);

    // Thin cover reads as pale blue, dense deposit as opaque white.
    let sparse = vec3f(0.42, 0.62, 0.86);
    let dense = vec3f(1.0, 1.0, 1.0);
    let color = mix(sparse, dense, density);
    let alpha = clamp(0.25 + 0.75 * density, 0.0, 1.0);

    textureStore(output_texture, id.xy, vec4f(color, alpha));
}

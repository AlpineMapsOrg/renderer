/*****************************************************************************
 * weBIGeo
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
 
@group(0) @binding(0) var input_tiles: texture_2d_array<u32>;
@group(0) @binding(1) var output_tiles: texture_storage_2d_array<rgba8unorm, write>;

@compute @workgroup_size(1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    for (var i: u32 = 0; i < 256; i++) {
        for (var j: u32 = 0; j < 256; j++) {
            var color = vec4f(f32(i) / 256, f32(j) / 256, f32(id.x) / 256, 1);
            textureStore(output_tiles, vec2(i, j), id.x, color);
        }
    }
}

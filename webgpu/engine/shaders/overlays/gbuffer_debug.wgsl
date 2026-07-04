/*****************************************************************************
* weBIGeo
* Copyright (C) 2026 Gerald Kimmersdorfer
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http : //www.gnu.org/licenses/>.
*****************************************************************************/

@group(0) @binding(0) var overlay_texture: texture_2d<u32>; //GBuffer slot 3 (packed RGBA via pack4x8unorm)
@group(0) @binding(1) var<uniform> settings: TileDebugSettings;
@group(0) @binding(2) var output_texture: texture_storage_2d<rgba8unorm, write>;
@group(0) @binding(3) var prev_output: texture_2d<f32>;

struct TileDebugSettings {
    strength: f32,
}

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    let dims = vec2u(textureDimensions(output_texture));
    if id.x >= dims.x || id.y >= dims.y {
        return;
    }
    let tci = id.xy;

    //render_tiles.wgsl writes alpha = 1 on geometry, 0 (transparent) on background.
    let packed = textureLoad(overlay_texture, tci, 0).r;
    var overlay_color = unpack4x8unorm(packed);
    overlay_color.a = overlay_color.a * settings.strength;

    //Blend over previous overlay in premultiplied alpha space:
    let prev = textureLoad(prev_output, tci, 0);
    let src_premul = vec4f(overlay_color.rgb * overlay_color.a, overlay_color.a);
    let blended = src_premul + prev * (1.0 - overlay_color.a);
    textureStore(output_texture, tci, blended);
}

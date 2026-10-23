/*****************************************************************************
* weBIGeo
* Copyright (C) 2026 Gerald Kimmersdorfer
* Copyright (C) 2024 Patrick Komon
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

///use util/shared_config
///use util/camera_config
///use webgpu::encoder
///use webgpu::tile_util
///use webgpu::snow

@group(0) @binding(0) var<uniform> conf: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
@group(2) @binding(0) var position_texture: texture_2d<f32>;
@group(2) @binding(1) var normal_texture: texture_2d<u32>;
@group(2) @binding(2) var<uniform> settings: ScreenSpaceSnowSettings;
@group(2) @binding(3) var output_texture: texture_storage_2d<rgba8unorm, write>;
@group(2) @binding(4) var prev_output: texture_2d<f32>;
@group(2) @binding(5) var gbuffer_overlay: texture_2d<u32>; //GBuffer slot 3

//Avalanche pixels (render_avalanche*.wgsl) carry this in the GBuffer overlay slot: the avalanche
//brings its own snow, so the overlay leaves it alone instead of painting over it.
const AVALANCHE_MARKER: u32 = 0x00FFFFFFu;
const PATCH_SCALE = 40.0; //world units across a snow patch (larger noise octave)
const PATCH_EDGE = 0.04; //soft patch edges, in noise units

struct ScreenSpaceSnowSettings {
    angle_min: f32,
    angle_max: f32,
    angle_blend: f32,
    altitude_limit: f32,
    altitude_variation: f32,
    altitude_blend: f32,
    transparency: f32,
    patches: f32, // 0: thin snow is translucent; 1: thin snow is sparse patches (see below)
}

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3u) {
    let dims = vec2u(textureDimensions(position_texture));
    if id.x >= dims.x || id.y >= dims.y {
        return;
    }
    let tci = id.xy;

    let pos_dist = textureLoad(position_texture, tci, 0);
    let encoded_normal = textureLoad(normal_texture, tci, 0).xy;

    let pos_cws = pos_dist.xyz;
    let dist = length(pos_cws);
    let pos_ws = pos_cws + camera.position.xyz;
    let normal = octNormalDecode2u16(encoded_normal);

    var out_color = vec4f(0.0);

    if dist > 0.0 && textureLoad(gbuffer_overlay, tci, 0).r != AVALANCHE_MARKER {
        //Reuse the shared snow math: angle limits live in .yzw, altitude params in .xyz.
        let angle = vec4f(0.0, settings.angle_min, settings.angle_max, settings.angle_blend);
        let altitude = vec4f(settings.altitude_limit, settings.altitude_variation, settings.altitude_blend, 0.0);
        out_color = overlay_snow(normal, pos_ws, angle, altitude);
        //Patches: read the falloff as the share of the ground that is covered, not as an opacity.
        //A pixel is snowy where world-space noise lies below that share, so thin snow becomes
        //sparse, irregular patches (as on a real snow line) that stay put on the ground. Far
        //away, where a noise cell shrinks below a few pixels, back to the average to avoid shimmer.
        if settings.patches > 0.0 {
            let n = 0.6 * noise(pos_ws / PATCH_SCALE) + 0.4 * noise(pos_ws / (0.3 * PATCH_SCALE) + 31.0);
            let patchy = smoothstep(n - PATCH_EDGE, n + PATCH_EDGE, out_color.a);
            let pixel = 2.0 * dist / (camera.viewport_size.y * camera.proj_matrix[1][1]);
            let resolved = 1.0 - smoothstep(0.1 * PATCH_SCALE, 0.4 * PATCH_SCALE, pixel);
            out_color.a = mix(out_color.a, patchy, settings.patches * resolved);
        }
        out_color.a *= settings.transparency;
    }

    //Blend over previous overlay in premultiplied alpha space:
    let prev = textureLoad(prev_output, tci, 0);
    let src_premul = vec4f(out_color.rgb * out_color.a, out_color.a);
    let blended = src_premul + prev * (1.0 - out_color.a);
    textureStore(output_texture, tci, blended);
}

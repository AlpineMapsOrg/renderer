/*****************************************************************************
* weBIGeo
* Copyright (C) 2026 Gerald Kimmersdorfer
* Copyright (C) 2025 Patrick Komon
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
///use screen_pass_vert

@group(0) @binding(0) var<uniform> conf: shared_config;
@group(1) @binding(0) var<uniform> camera: camera_config;
@group(2) @binding(0) var position_texture: texture_2d<f32>;
@group(2) @binding(1) var<uniform> settings: TextureOverlaySettings;
@group(2) @binding(2) var overlay_texture: texture_2d<f32>;
@group(2) @binding(3) var overlay_sampler: sampler;
@group(2) @binding(4) var background: texture_2d<f32>; //ping-pong: previous overlay state (premultiplied)

struct TextureOverlaySettings {
    aabb_min: vec2f,
    aabb_size: vec2f,
    opacity: f32,
    mode: u32,
    float_decode_range: vec2f,
    encoded_float_range: vec2f,
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    let tci = vec2i(in.position.xy);
    let pos_dist = textureLoad(position_texture, tci, 0);
    let pos_cws = pos_dist.xyz;
    let dist = length(pos_cws);
    let pos_ws = pos_cws + camera.position.xyz;

    let uv = vec2f(
        (pos_ws.x - settings.aabb_min.x) / settings.aabb_size.x,
        1.0 - (pos_ws.y - settings.aabb_min.y) / settings.aabb_size.y
    );
    let ddx_uv = dpdx(uv);
    let ddy_uv = dpdy(uv);

    var src = vec4f(0.0);
    let aabb_max = settings.aabb_min + settings.aabb_size;
    if !(dist <= 0.0 || any(pos_ws.xy < settings.aabb_min) || any(pos_ws.xy > aabb_max)) {
        let sample = textureSampleGrad(overlay_texture, overlay_sampler, uv, ddx_uv, ddy_uv);
        if settings.mode == 1u {
            //EncodedFloat: RGBA encodes a u32 via (r<<24|g<<16|b<<8|a), decoded over
            //settings.encoded_float_range, then normalized to settings.float_decode_range.
            let rgba_u8 = vec4u(sample * 255.0);
            let packed = (rgba_u8.r << 24u) | (rgba_u8.g << 16u) | (rgba_u8.b << 8u) | rgba_u8.a;
            let value = u32_to_range(packed, settings.encoded_float_range);
            let t = (value - settings.float_decode_range.x) / (settings.float_decode_range.y - settings.float_decode_range.x);
            if t > 0.0 && t < 1.0 {
                src = vec4f(vec3f(1.0 - t, 0.0, 0.0) * settings.opacity, settings.opacity);
            }
        } else {
            //AlphaBlend (mode == 0): premultiplied-alpha.
            let eff_a = sample.a * settings.opacity;
            src = vec4f(sample.rgb * eff_a, eff_a);
        }
    }

    //Blend over previous overlay in premultiplied alpha space:
    let bg = textureLoad(background, tci, 0);
    return vec4f(src.rgb + bg.rgb * (1.0 - src.a), src.a + bg.a * (1.0 - src.a));
}

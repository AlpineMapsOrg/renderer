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

#include "util/shared_config.wgsl"
#include "util/camera_config.wgsl"
#include "util/encoder.wgsl"
#include "util/tile_util.wgsl"

@group(0) @binding(0) var<uniform> conf : shared_config;
@group(1) @binding(0) var<uniform> camera : camera_config;
@group(2) @binding(0) var position_texture : texture_2d<f32>;
@group(2) @binding(1) var normal_texture : texture_2d<u32>;
@group(2) @binding(2) var<uniform> settings : HeightLinesSettings;
@group(2) @binding(3) var output_texture : texture_storage_2d < rgba8unorm, write>;
@group(2) @binding(4) var prev_output : texture_2d<f32>;

struct HeightLinesSettings {
    primary_interval : f32,
    secondary_interval : f32,
    base_width : f32,
    minor_opacity : f32,
    line_color : vec4f,
}

//Returns the blending alpha for a height line at this pixel, or 0 if no line.
fn get_height_line_alpha(pos_ws : vec3f, normal : vec3f, dist : f32, interval : f32, base_width : f32, aa_scale : f32) -> f32 {
    let alpha_line = 1.0 - min(dist / 20000.0, 1.0);
    if (alpha_line <= 0.01)
    {
        return 0.0;
    }

    var line_width = (1.0 + dist / 5000.0) * base_width;
    var aa_scaled = 0.0;
    if (aa_scale > 0.0)
    {
        aa_scaled = max(dist / 2000.0 * aa_scale, 0.03);
    }

    let steepness = acos(clamp(normal.z, -1.0, 1.0));
    line_width = line_width * max(0.01, steepness);

    let latitude_correction = cos(y_to_lat(pos_ws.y));
    let altitude = pos_ws.z * latitude_correction;
    let fractional_part = altitude - f32(i32(altitude / interval)) * interval;
    let dist_from_line = min(fractional_part, interval - fractional_part);
    let dist_from_edge = (line_width / 2.0) - dist_from_line;

    if (abs(dist_from_edge) < aa_scaled)
    {
        let aa_factor = smoothstep(-aa_scaled, aa_scaled, dist_from_edge);
        let effective_alpha = alpha_line * aa_factor;
        if (effective_alpha > 0.01)
        {
            return effective_alpha;
        }
    } else if (dist_from_edge > 0.0)
    {
        return alpha_line;
    }
    return 0.0;
}

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id : vec3u)
{
    let dims = vec2u(textureDimensions(position_texture));
    if (id.x >= dims.x || id.y >= dims.y)
    {
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

    if (dist > 0.0)
    {
        let major = get_height_line_alpha(pos_ws, normal, dist, settings.primary_interval, settings.base_width, 1.0);
        let minor = get_height_line_alpha(pos_ws, normal, dist, settings.secondary_interval, settings.base_width * 0.5, 1.0);
        let alpha = max(major, minor * settings.minor_opacity) * settings.line_color.a;
        out_color = vec4f(settings.line_color.rgb, alpha);
    }

    //Blend over previous overlay in premultiplied alpha space:
    let prev = textureLoad(prev_output, tci, 0);
    let src_premul = vec4f(out_color.rgb * out_color.a, out_color.a);
    let blended = src_premul + prev * (1.0 - out_color.a);
    textureStore(output_texture, tci, blended);
}

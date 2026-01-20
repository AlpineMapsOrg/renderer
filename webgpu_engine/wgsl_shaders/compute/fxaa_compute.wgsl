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

// input
@group(0) @binding(0) var input_texture: texture_2d<f32>;
@group(0) @binding(1) var input_sampler: sampler;

// output
@group(0) @binding(2) var output_texture: texture_storage_2d<rgba8unorm, write>; // ASSERT: same dimensions as input_texture

const FXAA_REDUCE_MIN: f32 = 1.0 / 256.0;
const FXAA_REDUCE_MUL: f32 = 1.0 / 8.0;
const FXAA_SPAN_MAX: f32 = 8.0;

// TODO copyright?
// adapted from here https://github.com/mattdesl/glsl-fxaa/blob/master/fxaa.glsl
fn fxaa(pos: vec2u) -> vec4f {
    let inverse_vp = vec2f(1.0) / vec2f(textureDimensions(input_texture));


    let pos_nw: vec2i = vec2i(pos) + vec2i(-1, -1);
    let pos_ne: vec2i = vec2i(pos) + vec2i( 1, -1);
    let pos_sw: vec2i = vec2i(pos) + vec2i(-1,  1);
    let pos_se: vec2i = vec2i(pos) + vec2i( 1,  1);

    let rgb_nw = textureLoad(input_texture, pos_nw, 0).rgb;
    let rgb_ne = textureLoad(input_texture, pos_ne, 0).rgb;
    let rgb_sw = textureLoad(input_texture, pos_sw, 0).rgb;
    let rgb_se = textureLoad(input_texture, pos_se, 0).rgb;
    let tex_color = textureLoad(input_texture, pos, 0).rgba;
    let rgb_m = tex_color.rgb;

    let luma = vec3f(0.299, 0.587, 0.114);
    let luma_nw = dot(rgb_nw, luma);
    let luma_ne = dot(rgb_ne, luma);
    let luma_sw = dot(rgb_sw, luma);
    let luma_se = dot(rgb_se, luma);
    let luma_m = dot(rgb_m, luma);

    let luma_min = min(luma_m, min(min(luma_nw, luma_ne), min(luma_sw, luma_se)));
    let luma_max = max(luma_m, max(max(luma_nw, luma_ne), max(luma_sw, luma_se)));

    var dir = vec2f(-((luma_nw + luma_ne) - (luma_sw + luma_se)), ((luma_nw + luma_sw) - (luma_ne + luma_se)));
    let dir_reduce = max((luma_nw + luma_ne + luma_sw + luma_se) *
                          (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

    let rcp_dir_min = 1.0 / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    
    dir = min(vec2f(FXAA_SPAN_MAX, FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcp_dir_min)) * inverse_vp;
    
    let uv = vec2f(pos) * inverse_vp;

    let rgb_a = 0.5 * (
        textureSampleLevel(input_texture, input_sampler, uv + dir * (1.0 / 3.0 - 0.5), 0.0).xyz +
        textureSampleLevel(input_texture, input_sampler, uv + dir * (2.0 / 3.0 - 0.5), 0.0).xyz);
        
    let rgb_b = 0.5 * (
        textureSampleLevel(input_texture, input_sampler, uv + dir * (1.0 / 3.0 - 0.5), 0.0).xyz +
        textureSampleLevel(input_texture, input_sampler, uv + dir * (2.0 / 3.0 - 0.5), 0.0).xyz);

    let luma_b = dot(rgb_b, luma);
    var color_output: vec4f;
    if (luma_b < luma_min || luma_b > luma_max) {
        //color_output = vec4f(rgb_a.rgb, tex_color.a);
        color_output = vec4f(rgb_a.rgb, length(rgb_a));
        //color_output = select(vec4f(rgb_a, length(rgb_a)), tex_color, tex_color.a > 0);
    } else {
        //color_output = vec4f(rgb_b.rgb, tex_color.a);
        color_output = vec4f(rgb_b.rgb, length(rgb_b));
        //color_output = select(vec4f(rgb_b, length(rgb_b)), tex_color, tex_color.a > 0);
    }

    // apply colors on top
    if (tex_color.a != 0.0) {
        color_output = vec4f(tex_color.rgb, tex_color.a);
    }

    return color_output;
}

//TODO copyright?
//adapted from godot engine, https://github.com/godotengine/godot/blob/3762b40de725ce33ae31f4af9b9c8e4a5d5b8861/drivers/gles3/shaders/tonemap.glsl
fn fxaa_with_transparency(pos: vec2<u32>, exposure: f32) -> vec4<f32> {
    let pixel_size = 1.0 / vec2f(textureDimensions(input_texture));

    let pos_nw: vec2i = vec2i(pos) + vec2i(-1, -1);
    let pos_ne: vec2i = vec2i(pos) + vec2i( 1, -1);
    let pos_sw: vec2i = vec2i(pos) + vec2i(-1,  1);
    let pos_se: vec2i = vec2i(pos) + vec2i( 1,  1);

    let rgbNW = textureLoad(input_texture, pos_nw, 0);
    let rgbNE = textureLoad(input_texture, pos_ne, 0);
    let rgbSW = textureLoad(input_texture, pos_sw, 0);
    let rgbSE = textureLoad(input_texture, pos_se, 0);
    let color = textureLoad(input_texture, pos, 0);

    let rgbM = color.rgb;
    let luma = vec3<f32>(0.299, 0.587, 0.114);

    let lumaNW = dot(rgbNW.rgb * exposure, luma) - ((1.0 - rgbNW.a) / 8.0);
    let lumaNE = dot(rgbNE.rgb * exposure, luma) - ((1.0 - rgbNE.a) / 8.0);
    let lumaSW = dot(rgbSW.rgb * exposure, luma) - ((1.0 - rgbSW.a) / 8.0);
    let lumaSE = dot(rgbSE.rgb * exposure, luma) - ((1.0 - rgbSE.a) / 8.0);
    let lumaM = dot(rgbM * exposure, luma) - (color.a / 8.0);

    let lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    let lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    var dir = vec2<f32>(0.0);
    dir.x = -(lumaNW + lumaNE - (lumaSW + lumaSE));
    dir.y = lumaNW + lumaSW - (lumaNE + lumaSE);

    let dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN
    );

    let rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(
        dir * rcpDirMin,
        vec2<f32>(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
        vec2<f32>(FXAA_SPAN_MAX, FXAA_SPAN_MAX)
    ) * pixel_size;

    let uv = vec2f(pos) * pixel_size;

    let rgbA: vec4f = 0.5 * exposure * (
        textureSampleLevel(input_texture, input_sampler, uv + dir * (1.0 / 3.0 - 0.5), 0).rgba +
        textureSampleLevel(input_texture, input_sampler, uv + dir * (2.0 / 3.0 - 0.5), 0).rgba
    );

    let rgbB: vec4f = rgbA * 0.5 + 0.25 * exposure * (
        textureSampleLevel(input_texture, input_sampler, uv + dir * -0.5, 0).rgba +
        textureSampleLevel(input_texture, input_sampler, uv + dir * 0.5, 0).rgba
    );

    let lumaB = dot(rgbB.rgb, luma) - ((1.0 - rgbB.a) / 8.0);
    var color_output: vec4f;
    if (lumaB < lumaMin || lumaB > lumaMax) {
        color_output = rgbA;
    } else {
        color_output = rgbB;
    }

    if (color_output.a == 0.0) {
        color_output = vec4f(0, 0, 0, 0);
    } else if (color_output.a < 1.0) {
        color_output = vec4f(color_output.rgb / color_output.a, color_output.a);
    }

    // apply colors on top
    if (color.a != 0.0) {
        color_output = vec4f(color.rgb, color.a);
    }

    return color_output;
}

@compute @workgroup_size(16, 16, 1)
fn computeMain(@builtin(global_invocation_id) id: vec3<u32>) {
    // id.xy in [0, ceil(texture_dimensions(normals_texture).xy / workgroup_size.xy) - 1]

    // exit if thread id is outside image dimensions (i.e. thread is not supposed to be doing any work)
    let texture_size = textureDimensions(input_texture);
    if (id.x >= texture_size.x || id.y >= texture_size.y) {
        return;
    }
    let texture_pos: vec2u = id.xy; // in [0, texture_dimensions(normals_texture) - 1]

    //let color = fxaa(texture_pos);
    let color = fxaa_with_transparency(texture_pos, 1.0);
    //let color = textureLoad(input_texture, texture_pos, 0).rgba;
    
    textureStore(output_texture, texture_pos, color);
}

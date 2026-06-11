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

fn bilinear_sample_vec4f(texture_array: texture_2d_array<f32>, texture_sampler: sampler, uv: vec2f, layer: u32) -> vec4f {
    let texture_dimensions: vec2u = textureDimensions(texture_array);

    // weights need to match the texels that are chosen by textureGather - this does NOT align perfectly, and introduces some artifacts
    // adding offset fixes this issue, see https://www.reedbeta.com/blog/texture-gathers-and-coordinate-precision/
    const offset = 1.0f / 512.0f;

    let weights: vec2f = fract(uv * vec2f(texture_dimensions) - 0.5 + offset);

    let x = dot(vec4f((1.0 - weights.x) * weights.y, weights.x * weights.y, weights.x * (1.0 - weights.y), (1.0 - weights.x) * (1.0 - weights.y)),
        vec4f(textureGather(0, texture_array, texture_sampler, uv, layer)));
    let y = dot(vec4f((1.0 - weights.x) * weights.y, weights.x * weights.y, weights.x * (1.0 - weights.y), (1.0 - weights.x) * (1.0 - weights.y)),
        vec4f(textureGather(1, texture_array, texture_sampler, uv, layer)));
    let z = dot(vec4f((1.0 - weights.x) * weights.y, weights.x * weights.y, weights.x * (1.0 - weights.y), (1.0 - weights.x) * (1.0 - weights.y)),
        vec4f(textureGather(2, texture_array, texture_sampler, uv, layer)));
    let w = dot(vec4f((1.0 - weights.x) * weights.y, weights.x * weights.y, weights.x * (1.0 - weights.y), (1.0 - weights.x) * (1.0 - weights.y)),
        vec4f(textureGather(3, texture_array, texture_sampler, uv, layer)));

    return vec4f(x, y, z, w);
}

fn bilinear_sample_u32(texture_array: texture_2d_array<u32>, texture_sampler: sampler, uv: vec2f, layer: u32) -> u32 {
    // weights need to match the texels that are chosen by textureGather - this does NOT align perfectly, and introduces some artifacts
    // adding offset fixes this issue, see https://www.reedbeta.com/blog/texture-gathers-and-coordinate-precision/
    const TEXTURE_GATHER_OFFSET = 1.0f / 512.0f;

    let texture_dimensions = textureDimensions(texture_array);
    let weights: vec2f = fract(uv * vec2f(texture_dimensions) - 0.5 + TEXTURE_GATHER_OFFSET); // -0.5 to make relative to texel center
    let texel_values = vec4f(textureGather(0, texture_array, texture_sampler, uv, layer));
    return u32(dot(vec4f((1.0 - weights.x) * weights.y, weights.x * weights.y, weights.x * (1.0 - weights.y), (1.0 - weights.x) * (1.0 - weights.y)), texel_values));
}
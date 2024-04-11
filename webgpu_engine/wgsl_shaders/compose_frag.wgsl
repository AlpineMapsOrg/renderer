#include "shared_config.wgsl"
#include "screen_pass_shared.wgsl"

@group(0) @binding(0) var<uniform> config : shared_config;
@group(1) @binding(0) var albedo_texture : texture_2d<f32>;
@group(1) @binding(1) var compose_sampler : sampler;

@fragment
fn fragmentMain(vertex_out : VertexOut) -> @location(0) vec4f {
    let test_col = vertex_out.texcoords.xy;
    let albedo = textureSample(albedo_texture, compose_sampler, vertex_out.texcoords.xy).rgb;

    const u = 0.5;
    let blend = u * vec3f(test_col, 0) + (1 - u) * albedo;

    return vec4f(blend, 1);
}
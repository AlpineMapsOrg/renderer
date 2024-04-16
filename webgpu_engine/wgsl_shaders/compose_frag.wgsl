#include "shared_config.wgsl"
#include "screen_pass_shared.wgsl"

@group(0) @binding(0) var<uniform> config : shared_config;
@group(1) @binding(0) var albedo_texture : texture_2d<f32>;
@group(1) @binding(1) var compose_sampler : sampler;

@fragment
fn fragmentMain(vertex_out : VertexOut) -> @location(0) vec4f {
    let albedo = textureSample(albedo_texture, compose_sampler, vertex_out.texcoords.xy).rgb;
    return vec4f(albedo, 1);
}
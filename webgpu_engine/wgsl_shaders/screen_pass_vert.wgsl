#include "screen_pass_shared.wgsl"

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index : u32) -> VertexOut {
    const VERTICES = array(
        vec2f(-1.0, -1.0),
        vec2f(3.0, -1.0),
        vec2f(-1.0, 3.0)
    );

    var vertex_out : VertexOut;
    vertex_out.position = vec4(VERTICES[vertex_index], 0.0, 1.0);
    vertex_out.texcoords = vec2(0.5, -0.5) * vertex_out.position.xy + vec2(0.5);
    return vertex_out;
}
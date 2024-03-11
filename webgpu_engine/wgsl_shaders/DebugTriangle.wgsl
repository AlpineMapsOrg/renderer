@vertex
fn vertexMain(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4f
{
    const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
    return vec4f(pos[vertex_index], 0, 1);
}

@fragment
fn fragmentMain() -> @location(0) vec4f
{
    return vec4f(0.0, 0.4, 1.0, 1.0);
}

out vec2 uv;

void main(){
    vec2[] vertices = vec2[](
        vec2(1, 0), vec2(1,1), vec2(0, 0), vec2(0, 1)
    );

    gl_Position = vec4(vertices[gl_VertexID] * 2 - 1, 1, 1);
    uv = vertices[gl_VertexID];
}

// https://stackoverflow.com/a/59739538
out highp vec2 texcoords; // texcoords are in the normalized [0,1] range for the viewport-filling quad part of the triangle
void main() {
    vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0),
                             vec2(3.0, -1.0),
                             vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 1.0, 1.0);
    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
}

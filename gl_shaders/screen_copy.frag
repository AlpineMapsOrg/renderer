in highp vec2 texcoords;
uniform sampler2D texture_sampler;
out lowp vec4 out_Color;
void main() {
    out_Color = vec4(texcoords.xy, 0.0, 1.0) * 0.1 + texture(texture_sampler, texcoords);
}

layout(binding = 0) uniform sampler2D texture_sampler;

in vec2 uv;

out lowp vec4 out_Color;

void main() {

    out_Color = vec4(texture(texture_sampler, uv).xyz, 1.f);
    //out_Color = vec4(uv, 0.f, 1.f);
}

uniform highp sampler2DArray source_texture;
uniform highp int source_layer;
uniform highp int source_level;
layout(location = 0) out highp vec4 out_color;

void main()
{
    out_color = texelFetch(source_texture, ivec3(ivec2(gl_FragCoord.xy), source_layer), source_level);
}

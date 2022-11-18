uniform highp vec3 camera_position;
uniform sampler2D texture_sampler;
in lowp vec2 uv;
in highp vec3 camera_rel_pos;
out lowp vec4 out_Color;
void main() {
   highp vec3 origin = vec3(camera_position);
   highp vec4 ortho = texture(texture_sampler, uv);
   highp float dist = length(camera_rel_pos) / 1000.0;
   out_Color = ortho * 1.0 + 0.0 * vec4(dist, dist, dist, 1.0);
   //gl_FragDepth = gl_FragCoord.z;
}

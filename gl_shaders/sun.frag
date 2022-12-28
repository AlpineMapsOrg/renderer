out highp vec4 depth;

void main() {
    depth = vec4(vec3(gl_FragCoord.z), 1.f);
}

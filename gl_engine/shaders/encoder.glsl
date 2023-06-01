lowp vec2 encode(highp float value) {
    mediump uint scaled = uint(value * 65535.f + 0.5f);
    mediump uint r = scaled >> 8u;
    mediump uint b = scaled & 255u;
    return vec2(float(r) / 255.f, float(b) / 255.f);
}

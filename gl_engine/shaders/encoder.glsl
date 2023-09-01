lowp vec2 encode(highp float value) {
    mediump uint scaled = uint(value * 65535.f + 0.5f);
    mediump uint r = scaled >> 8u;
    mediump uint b = scaled & 255u;
    return vec2(float(r) / 255.f, float(b) / 255.f);
}

highp float d_2u8_to_f16(mediump uint v1, mediump uint v2) {
    return ((v1 << 8u) | v2 ) / 65535.f;
}

highp float decode(lowp vec2 value) {
    mediump uint r = mediump uint(value.x * 255.0f);
    mediump uint g = mediump uint(value.y * 255.0f);
    return d_2u8_to_f16(r,g);
}

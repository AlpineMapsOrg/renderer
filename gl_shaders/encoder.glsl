highp vec2 encode(highp float value) {
    int scaled = int(value * 65535.f + 0.5f);
    int r = scaled >> 8;
    int b = scaled & 255;
    return vec2(float(r) / 255.f, float(b) / 255.f);
}


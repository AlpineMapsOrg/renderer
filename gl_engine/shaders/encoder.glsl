
// ===== DEPTH ENCODE =====
// NOTE: Only positions up to e^13 = 442413 are in between [0,1]
// Thats not perfect, but I'll leave it as it is...
// Better would be to normalize it in between near and farplane
// or just use the cs Depth immediately.
highp float fakeNormalizeWSDepth(highp float depth) {
    return log(depth) / 13.0;
}
highp float fakeUnNormalizeWSDepth(highp float ndepth) {
    return exp(13.0 * ndepth);
}
highp uint depthWSEncode1u32(highp float depth) {
    return uint(fakeNormalizeWSDepth(depth) * 4294967295.0);
}
highp float depthWSDecode1u32(highp uint depth32) {
    return fakeUnNormalizeWSDepth(float(depth32) / 4294967295.0);
}


// ===== OCTAHEDRON MAPPING FOR NORMALS =====
// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
highp vec2 octWrap( highp vec2 v ) {
    return ( 1.0 - abs( v.yx ) ) * ( ( v.x >= 0.0 && v.y >= 0.0 ) ? 1.0 : -1.0 );
}

highp uvec2 octNormalEncode2u16(highp vec3 normal) {
    normal /= ( abs( normal.x ) + abs( normal.y ) + abs( normal.z ) );
    if (normal.z < 0.0) normal.xy = octWrap( normal.xy );
    normal.xy = normal.xy * 0.5 + 0.5;
    return uvec2(normal * 65535.0);
}

highp vec3 octNormalDecode2u16(highp uvec2 octNormal16) {
    highp vec2 f = vec2(octNormal16) / 65535.0 * 2.0 - 1.0;
    highp vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    highp float t = clamp( -n.z, 0.0, 1.0 );
    n.xy += ( n.x >= 0.0 && n.y >= 0.0) ? -t : t;
    return normalize( n );
}

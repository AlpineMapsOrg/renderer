lowp vec2 encode(highp float value) {
    mediump uint scaled = uint(value * 65535.f + 0.5f);
    mediump uint r = scaled >> 8u;
    mediump uint b = scaled & 255u;
    return vec2(float(r) / 255.f, float(b) / 255.f);
}

highp float d_2u8_to_f16(mediump uint v1, mediump uint v2) {
    return float((v1 << 8u) | v2 ) / 65535.f;
}

highp float decode(lowp vec2 value) {
    mediump uint r = uint(value.x * 255.0f);
    mediump uint g = uint(value.y * 255.0f);
    return d_2u8_to_f16(r,g);
}

// DEPTH ENCODE
highp float linearize_depth(highp float d,highp float zNear, highp float zFar)
{
    return (2.0 * zNear) / (zFar + zNear - d * (zFar - zNear));
}

highp uint depthCSEncode1u32(highp float depth) {
    depth = linearize_depth(depth, 1.0f, 1000000000.0f);
    return uint(depth * 4294967295.0);
}

highp float depthCSDecode1u32(highp uint depth32) {
    return float(depth32) / 4294967295.0;
}

// OCTAHEDRON MAPPING FOR NORMALS
// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
highp vec2 octWrap( highp vec2 v )
{
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

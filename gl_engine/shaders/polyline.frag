#include "intersection.glsl"
#include "hashing.glsl"
#include "encoder.glsl"

layout (location = 0) out lowp vec3 texout_albedo;
layout (location = 1) out highp vec4 texout_position;
layout (location = 2) out highp uvec2 texout_normal;
layout (location = 3) out highp uint texout_depth;
layout (location = 4) out highp uint texout_vertex_id;

in highp vec3 color;

flat in highp int vertex_id;

uniform highp sampler2D texin_track;
uniform highp sampler2D texin_position;
uniform highp vec3 camera_position;

uniform highp vec2 resolution;

void main() {
    texout_vertex_id = uint(vertex_id);
    //texout_albedo = color;
    // intersect here?
    // attenuation?
    // specular hightlight?

    //texout_albedo = vec4(color_from_id_hash(vertex_id), 1).rgb;



    vec2 texcoords = gl_FragCoord.xy / resolution.xy;
    
    //texout_albedo = vec3(texcoords, 0);

#if 1 // intersect in fragment shader

    // track vertex position
    highp vec3 track_vert = texelFetch(texin_track, ivec2(int(vertex_id - 1), 0), 0).xyz; 
    highp vec3 next_track_vert = texelFetch(texin_track, ivec2(int(vertex_id), 0), 0).xyz; 
    highp vec3 prev_track_vert = texelFetch(texin_track, ivec2(int(vertex_id - 2), 0), 0).xyz; 
    

    highp vec4 pos_dist = texture(texin_position, texcoords);
    highp vec3 pos_cws = pos_dist.xyz;
    highp float dist = pos_dist.w; // negative if sky

    Sphere sphere;
    sphere.position = track_vert;
    sphere.radius = 7;

    Ray ray;
    highp vec3 origin = vec3(camera_position);
    highp vec3 pos_ws = pos_cws + origin;
    ray.origin = origin;
    ray.direction = pos_cws / dist;

    float t = INF;
    vec3 point;

    if (IntersectRaySphere(ray, sphere, t, point)) {
        texout_albedo = vec3(0,1,0);

        // highp float dist = length(point);
        // texout_depth = depthWSEncode1u32(dist);
    } else {
        discard;
    }

#endif





}
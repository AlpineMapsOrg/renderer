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
uniform bool enable_intersection;

uniform highp vec2 resolution;

bool check_collision(float t) {
    return 0 < t && t < INF;
}

void main() {
    texout_vertex_id = uint(vertex_id);
    // intersect here?
    // attenuation?
    // specular hightlight?

    if (!enable_intersection) {
        texout_albedo = vec3(1,0,0);
    } else {

    //texout_albedo = vec4(color_from_id_hash(vertex_id), 1).rgb;



    vec2 texcoords = gl_FragCoord.xy / resolution.xy;

    //texout_albedo = vec3(texcoords, 0);

#if 1 // intersect in fragment shader

    // track vertex position
    uint id = vertex_id / 2;
    highp vec3 track_vert = texelFetch(texin_track, ivec2(int(id), 0), 0).xyz;
    highp vec3 next_track_vert = texelFetch(texin_track, ivec2(int(id + 1), 0), 0).xyz;
    highp vec3 prev_track_vert = texelFetch(texin_track, ivec2(int(id - 1), 0), 0).xyz;

    highp vec4 pos_dist = texture(texin_position, texcoords);
    highp vec3 terrain_pos = pos_dist.xyz;
    highp float dist = pos_dist.w; // negative if sky

    Ray ray;
    highp vec3 origin = vec3(camera_position);
    highp vec3 pos_ws = terrain_pos + origin;
    ray.origin = origin;
    ray.direction = terrain_pos / dist;

#if 0
    Sphere sphere;
    sphere.position = track_vert;
    sphere.radius = 7;

    float t = INF;
    vec3 point;

    if (IntersectRaySphere(ray, sphere, t, point)) {
        highp vec3 normal = (point - sphere.position) / sphere.radius;
        highp vec3 normal_color = (normal + vec3(1)) / vec3(2);
        texout_albedo = normal_color;
    } else {
        discard;
    }
#else
    float capsule_radius = 5;
    Capsule c1;
    c1.p = track_vert;
    c1.q = next_track_vert;
    c1.radius = capsule_radius;

    float t1 = intersect_capsule(ray.origin, ray.direction, c1.p, c1.q, c1.radius);

    Capsule c2;
    c2.p = prev_track_vert;
    c2.q = track_vert;
    c2.radius = capsule_radius;

    float t2 = intersect_capsule(ray.origin, ray.direction, c2.p, c2.q, c2.radius);



    if ((0 < t1 && t1 < INF) || (0 < t2 && t2 < INF)) {

        float t;
        vec3 normal;


        if (t1 < t2 && 0 < t1) {
            t = t1;
            vec3 point = ray.origin + ray.direction * t;
            normal = capsule_normal(point, c1.p, c1.q,  capsule_radius);
        } else {
            t = t2;
            vec3 point = ray.origin + ray.direction * t;
            normal = capsule_normal(point, c2.p, c2.q,  capsule_radius);
        }

        vec3 red = vec3(1,0,0);
        vec3 blue = vec3(0,0,1);

        float diff = dist - t;

        if (diff > 0) {
            texout_albedo = mix(red, blue, diff / (capsule_radius * 2));
            //texout_albedo = normal;
        } else {
            texout_albedo = red;
            //discard;
        }

        //texout_albedo = vec3(0,1,0);
    } else {
        discard;
    }

#endif

#endif


    }



}
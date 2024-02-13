#include "intersection.glsl"
#include "hashing.glsl"
#include "encoder.glsl"

layout (location = 0) out lowp vec3 texout_albedo;
layout (location = 1) out highp vec4 texout_position;
layout (location = 2) out highp uvec2 texout_normal;
layout (location = 3) out highp uint texout_depth;
layout (location = 4) out highp uint texout_vertex_id;

#define SPHERE      0
#define CAPSULE     1
#define CAPSULE_v2  2

#define GEOMETRY    CAPSULE

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

        texout_albedo = vec4(color_from_id_hash(vertex_id), 1).rgb;

    } else {


    vec2 texcoords = gl_FragCoord.xy / resolution.xy;

#if 1 // intersect in fragment shader

    // track vertex position
    uint id = vertex_id;

    highp vec3 x0 = texelFetch(texin_track, ivec2(int(id - 1), 0), 0).xyz;
    highp vec3 x1 = texelFetch(texin_track, ivec2(int(id + 0), 0), 0).xyz;
    highp vec3 x2 = texelFetch(texin_track, ivec2(int(id + 1), 0), 0).xyz;
    highp vec3 x3 = texelFetch(texin_track, ivec2(int(id + 2), 0), 0).xyz;

    highp vec4 pos_dist = texture(texin_position, texcoords);
    highp vec3 terrain_pos = pos_dist.xyz;
    highp float dist = pos_dist.w; // negative if sky

    Ray ray;
    highp vec3 origin = vec3(camera_position);
    highp vec3 pos_ws = terrain_pos + origin;
    ray.origin = origin;
    ray.direction = terrain_pos / dist;

    float radius = 7;

#if (GEOMETRY == SPHERE)
    Sphere sphere = Sphere(x1, radius);

    float t = INF;
    vec3 point;

    if (IntersectRaySphere(ray, sphere, t, point)) {
        highp vec3 normal = (point - sphere.position) / sphere.radius;
        highp vec3 normal_color = (normal + vec3(1)) / vec3(2);
        texout_albedo = normal_color;
    } else {
        discard;
    }
#elif (GEOMETRY == CAPSULE)

    Capsule c1 = Capsule(x1, x2, radius);

    float t1 = intersect_capsule(ray.origin, ray.direction, c1.p, c1.q, c1.radius);

    Capsule c2 = Capsule(x0, x1, radius);

    float t2 = intersect_capsule(ray.origin, ray.direction, c2.p, c2.q, c2.radius);

    float t = -1;
    vec3 normal;

    if ((0 < t1 && t1 < INF) && (0 < t2 && t2 < INF)) {
        // intersect both
        texout_albedo = vec3(1,0,0);

        if (t1 < t2) {
            t = t1;
            normal = capsule_normal(ray.origin + ray.direction * t, c1.p, c1.q, c1.radius);
        } else {
            t = t2;
            normal = capsule_normal(ray.origin + ray.direction * t, c2.p, c2.q, c2.radius);
        }


    } else if ((0 < t1 && t1 < INF)) {
        texout_albedo = vec3(0,1,0);
        t = t1;
        normal = capsule_normal(ray.origin + ray.direction * t, c1.p, c1.q, c1.radius);

    } else if ((0 < t2 && t2 < INF)) {
        texout_albedo = vec3(0,0,1);
        t = t2;
        normal = capsule_normal(ray.origin + ray.direction * t, c2.p, c2.q, c2.radius);


    } else {
        discard; // no intersection
    }

    texout_albedo = normal;



#else
#error unknown GEOMETRY
#endif

#endif


    }



}
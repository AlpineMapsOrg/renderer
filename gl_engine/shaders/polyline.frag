#include "intersection.glsl"
#include "hashing.glsl"
#include "encoder.glsl"
#include "shared_config.glsl"
#include "turbo_colormap.glsl"

layout (location = 0) out lowp vec4 out_color;

#define SPHERE      0
#define CAPSULE     1

#define GEOMETRY    1

in highp vec3 color;

flat in highp int vertex_id;
flat in float radius;

uniform highp sampler2D texin_track;
uniform highp sampler2D texin_position;
uniform highp vec3 camera_position;
uniform bool enable_intersection;
uniform int shading_method;

uniform highp vec2 resolution;

vec3 visualize_normal(in vec3 normal) {
    return (normal + vec3(1.0)) / vec3(2.0);
}

vec3 phong_lighting(in vec3 albedo, in vec3 normal) {
    vec4 dirLight = conf.sun_light;
    vec4 ambLight  =  conf.amb_light;

    vec3 material = vec3(1.0);

    vec3 dirColor = dirLight.rgb * dirLight.a;
    vec3 ambColor = ambLight.rgb * ambLight.a;

    vec3 ambient = material.r * albedo;
    vec3 diff = material.g * albedo;
    vec3 spec = vec3(material.b);

    vec3 sun_light_dir = conf.sun_light_dir.xyz;

    float lambertian_coeff = max(0.0, dot(normal, sun_light_dir));

    return (ambient * ambColor) + (dirColor * lambertian_coeff);
}



void main() {

    if (!enable_intersection) {
        // only for debugging
        out_color = vec4(color_from_id_hash(uint(vertex_id)), 1);

    } else {

    vec2 texcoords = gl_FragCoord.xy / resolution.xy;

    vec3 sun_light_dir = conf.sun_light_dir.xyz;

#if 1 // intersect in fragment shader

    // track vertex position
    int id = vertex_id;

    highp vec4 p1 = texelFetch(texin_track, ivec2(int(id + 0), 0), 0);
    highp vec4 p2 = texelFetch(texin_track, ivec2(int(id + 1), 0), 0);

    highp vec3 x0 = texelFetch(texin_track, ivec2(int(id - 1), 0), 0).xyz;
    highp vec3 x1 = p1.xyz;
    highp vec3 x2 = p2.xyz;
    highp vec3 x3 = texelFetch(texin_track, ivec2(int(id + 2), 0), 0).xyz;

    highp float delta_time = p2.w;

    highp vec4 pos_dist = texture(texin_position, texcoords);

    // terrain position
    highp vec3 terrain_pos = pos_dist.xyz;

    // distance from camera to terrain, negative if sky
    highp float dist = pos_dist.w;

    bool sky = dist < 0;

    if (sky) {
        out_color = vec4(0,1,0,1);
        return;
    }

    // we need some better way of generating this ray
    Ray ray = Ray(camera_position, normalize(terrain_pos / abs(dist)));
    //Ray ray = Ray(camera_position, normalize(camera_position - terrain_pos));

#if (GEOMETRY == SPHERE)
    Sphere sphere = Sphere(x1, radius);

    float t = INF;
    vec3 point;

    if (IntersectRaySphere(ray, sphere, t, point)) {
        highp vec3 normal = (point - sphere.position) / sphere.radius;
        out_color = vec4(visualize_normal(normal), 1);
    } else {
        discard;
    }
#elif (GEOMETRY == CAPSULE)

    Capsule c = Capsule(x1, x2, radius);

    float t = intersect_capsule_2(ray, c);

    if ((0 < t && t < INF)) {

        vec3 point = ray.origin + ray.direction * t;

        vec3 normal = capsule_normal_2(point, c);

        Plane clipping_plane_1, clipping_plane_2;

        vec3 n0 = -normalize(x2 - x0);
        clipping_plane_1.normal = n0;
        clipping_plane_1.distance = dot(x1, n0);

        vec3 n1 = -normalize(x3 - x1);
        clipping_plane_2.normal = n1;
        clipping_plane_2.distance = dot(x2, n1);

        if (
            (0.0 >= signed_distance(point, clipping_plane_1) || (vertex_id == 0)) &&
            (0.0 <= signed_distance(point, clipping_plane_2)) // TODO: handle end of tube
        ) {

            vec3 color;

            vec3 RED = vec3(1,0,0);
            vec3 BLUE = vec3(0,0,1);

            vec3 A = x1;
            vec3 AP = point - x1;
            vec3 AB = x2 - x1;

            float f = dot(AP,AB) / dot(AB,AB);

            if (shading_method == 0) { // default

                color = phong_lighting(vec3(1,0,0), normal);

            } else if (shading_method == 1) { // normal

                color = visualize_normal(normal);

            } else if (shading_method == 2) { // speed

                float speed_0 = length(x0 - x1) / p1.w;
                float speed_1 = length(x1 - x2) / p2.w;

                float max_speed = 0.005; // TODO: should be dynamic

                float t_0 = clamp(speed_0 / max_speed, 0, 1);
                float t_1 = clamp(speed_1 / max_speed, 0, 1);

                color = mix(TurboColormap(t_0), TurboColormap(t_1), f);

            } else if (shading_method == 3) {  // steepness
                vec3 d1 = normalize(x0 - x1);
                vec3 d2 = normalize(x1 - x2);

                vec3 up = vec3(0,0,1);

                color = mix( TurboColormap(abs(dot(d1, up))), TurboColormap(abs(dot(d2, up))), f);

            } else {
                color = vec3(0);
            }

            if (t < dist) {
                // geometry is above terrain
                out_color = vec4(color, 1);
#if 0
            } else if ((t - dist) <= c.radius * 2) {

                float delta = (t - dist) / (c.radius * 2);

                out_color = vec4(color, 0.3);
#endif
            } else {
                out_color = vec4(color, 0.3);
                //discard; // geometry is far below terrain
            }
        } else {
            discard; // clipping
        }
    } else {
        discard; // no intersection
    }

#else
#error unknown GEOMETRY
#endif
#endif
    }
}

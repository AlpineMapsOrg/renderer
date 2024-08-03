/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2024 Jakob Maier
 * Copyright (C) 2024 Adam Celarek
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "intersection.glsl"
#include "hashing.glsl"
#include "encoder.glsl"
#include "shared_config.glsl"
#include "turbo_colormap.glsl"

layout (location = 0) out lowp vec4 out_color;

flat in highp int vertex_id;

uniform highp sampler2D texin_track;
uniform highp sampler2D texin_position;
uniform highp vec3 camera_position;
uniform bool enable_intersection;
uniform int shading_method;
uniform highp mat4 view;
uniform highp mat4 proj;
uniform highp float max_speed;
uniform highp float max_vertical_speed;
uniform highp float width;
uniform int end_index;


uniform highp vec2 resolution;

highp vec3 visualize_normal(highp vec3 normal) {
    return (normal + vec3(1.0)) / vec3(2.0);
}

// https://sibaku.github.io/computer-graphics/2017/01/10/Camera-Ray-Generation.html
highp vec3 camera_ray(highp vec2 px, highp mat4 inverse_proj, highp mat4 inverse_view)
{
    highp vec2 pxNDS =  2. * px - 1.;
    highp vec3 pointNDS = vec3(pxNDS, -1.);
    highp vec4 pointNDSH = vec4(pointNDS, 1.0);

    highp vec4 eye_dir = inverse_proj * pointNDSH;
    eye_dir.w = 0.;

    highp vec3 world_dir = (inverse_view * eye_dir).xyz;

    return normalize(world_dir);
}

highp vec3 calc_blinn_phong_contribution(highp vec3 toLight, highp vec3 toEye, highp vec3 normal, highp vec3 diffFactor, highp vec3 specFactor, highp float specShininess)
{
    highp float nDotL = max(0.0, dot(normal, toLight)); // lambertian coefficient
    highp vec3 h = normalize(toLight + toEye);
    highp float nDotH = max(0.0, dot(normal, h));
    highp float specPower = pow(nDotH, specShininess);
    highp vec3 diffuse = diffFactor * nDotL; // component-wise product
    highp vec3 specular = specFactor * specPower;
    return diffuse + specular;
}

highp vec3 phong_lighting(highp vec3 albedo, highp vec3 normal, highp vec3 eyePos, highp vec3 fragPos) {
    highp vec4 dirLight = conf.sun_light;
    highp vec4 ambLight  =  conf.amb_light;
    highp vec3 sun_light_dir = conf.sun_light_dir.xyz;
    highp vec4 material = conf.material_light_response;

    highp vec3 dirColor = dirLight.rgb * dirLight.a;
    highp vec3 ambColor = ambLight.rgb * ambLight.a;

    highp vec3 ambient = material.r * albedo;
    highp vec3 diff = material.g * albedo;
    highp vec3 spec = vec3(material.b);
    highp float shini = material.a;

    highp vec3 toLightDirWS = -normalize(conf.sun_light_dir.xyz);
    highp vec3 toEyeNrmWS = normalize(eyePos - fragPos);

    highp vec3 diffAndSpecIllumination = dirColor * calc_blinn_phong_contribution(toLightDirWS, toEyeNrmWS, normal, diff, spec, shini);

    return (ambient * ambColor) + diffAndSpecIllumination;
}

void main() {

    if (!enable_intersection) {
        // only for debugging
        out_color = vec4(color_from_id_hash(uint(vertex_id)), 1);

    } else {

        highp vec2 texcoords = gl_FragCoord.xy / resolution.xy;

        highp vec3 sun_light_dir = conf.sun_light_dir.xyz;

        // track vertex position
        int id = vertex_id;

        highp vec4 p1 = texelFetch(texin_track, ivec2(int(id + 0), 0), 0);
        highp vec4 p2 = texelFetch(texin_track, ivec2(int(id + 1), 0), 0);

        highp vec3 x0 = texelFetch(texin_track, ivec2(int(id - 1), 0), 0).xyz - camera_position;
        highp vec3 x1 = p1.xyz - camera_position;
        highp vec3 x2 = p2.xyz - camera_position;
        highp vec3 x3 = texelFetch(texin_track, ivec2(int(id + 2), 0), 0).xyz - camera_position;

        highp float delta_time = p2.w;

        highp vec4 pos_dist = texture(texin_position, texcoords);

        // terrain position
        highp vec3 terrain_pos = pos_dist.xyz;

        // distance from camera to terrain, negative if sky
        highp float dist = pos_dist.w;

        Ray ray;

        if (dist < 0.) {
            // ray does not hit terrain, it hits sky

            highp vec3 dir = camera_ray(texcoords, inverse(proj), inverse(view));
            ray = Ray(vec3(0, 0, 0), dir);
            dist = INF;

        } else {
            // ray does hit terrain
            ray = Ray(vec3(0, 0, 0), normalize(terrain_pos / abs(dist)));
        }

        Capsule c = Capsule(x1, x2, width);

        highp float t = intersect_capsule_2(ray, c);

        if ((0.0 < t && t < INF)) {

            highp vec3 point = ray.origin + ray.direction * t;

            highp vec3 normal = capsule_normal_2(point, c);

            Plane clipping_plane_1, clipping_plane_2;

            highp vec3 n0 = -normalize(x2 - x0);
            clipping_plane_1.normal = n0;
            clipping_plane_1.distance = dot(x1, n0);

            highp vec3 n1 = -normalize(x3 - x1);
            clipping_plane_2.normal = n1;
            clipping_plane_2.distance = dot(x2, n1);

            if (
                (0.5 >= signed_distance(point, clipping_plane_1) || (vertex_id == 0)) &&
                (-0.5 <= signed_distance(point, clipping_plane_2) || (vertex_id == end_index - 1))
            )
            {

                highp vec3 color;

                highp vec3 RED = vec3(1., 0., 0.);
                highp vec3 BLUE = vec3(0., 0., 1.);

                highp vec3 A = x1;
                highp vec3 AP = point - x1;
                highp vec3 AB = x2 - x1;

                // where point is along the capsule major axis
                highp float f = dot(AP, AB) / dot(AB, AB);

                if (shading_method == 0) { // default
                    color = vec3(1,0,0);

                } else if (shading_method == 1) { // normal

                    color = visualize_normal(normal);

                } else if (shading_method == 2) { // speed

                    highp float speed_0 = length(x0 - x1) / p1.w;
                    highp float speed_1 = length(x1 - x2) / p2.w;

                    highp float t_0 = clamp(speed_0 / max_speed, 0., 1.);
                    highp float t_1 = clamp(speed_1 / max_speed, 0., 1.);

                    color = mix(TurboColormap(t_0), TurboColormap(t_1), f);

                } else if (shading_method == 3) {  // steepness
                    highp vec3 d1 = normalize(x0 - x1);
                    highp vec3 d2 = normalize(x1 - x2);

                    highp vec3 up = vec3(0.0, 0.0, 1.0);

                    color = mix( TurboColormap(abs(dot(d1, up))), TurboColormap(abs(dot(d2, up))), f);

                } else if (shading_method == 4) {  // vertical speed

                    highp float speed_0 = abs(x0.z - x1.z) / p1.w;
                    highp float speed_1 = abs(x1.z - x2.z) / p2.w;

                    highp float t_0 = clamp(speed_0 / max_vertical_speed, 0., 1.);
                    highp float t_1 = clamp(speed_1 / max_vertical_speed, 0., 1.);

                    color = mix(TurboColormap(t_0), TurboColormap(t_1), f);

                } else {
                    color = vec3(0);
                }

                color = phong_lighting(color, normal, vec3(0, 0, 0), point);

                if (t < dist) {
                    // geometry is above terrain
                    out_color = vec4(color, 1.0);

                } else {

                    // track has constant alpha upto min_below, then it slowly falls off
                    highp float distance_below = t - dist;
                    highp float min_below = 100.0;
                    highp float max_below = 2000.0;

                    if (distance_below < min_below) {
                        out_color = vec4(color, 0.5);
                    } else {
                        highp float t = (distance_below - min_below) / (max_below - min_below);
                        out_color = vec4(color, 0.5 * (1.0 - t));
                    }
                }
            } else {
                discard; // clipping
            }
        } else {
            discard; // no intersection
        }


    }
}

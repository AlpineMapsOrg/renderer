#line 1
uniform highp vec3 camera_position;
uniform highp mat4 inversed_projection_matrix;
uniform highp mat4 inversed_view_matrix;
uniform sampler2D lookup_sampler;
in lowp vec2 texcoords;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

const highp float infinity = 1.0 / 0.0;

highp vec3 unproject(vec2 normalised_device_coordinates) {
   highp vec4 unprojected = inversed_projection_matrix * vec4(normalised_device_coordinates, 1.0, 1.0);
   highp vec4 normalised_unprojected = unprojected / unprojected.w;
   
   return normalize(vec3(inversed_view_matrix * normalised_unprojected));
}

// based on https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
highp float ray_sphere_intersect(highp vec2 ray_origin, highp vec2 ray_direction, highp float radius) {
    highp float t_ca = dot(ray_direction, -ray_origin);
    if (t_ca < 0 && dot(ray_origin, ray_origin) > radius * radius)
        return infinity; // pointing up, into the sky -> legal only inside the (outer) sphere
    highp float d = sqrt(max(0.0, dot(-ray_origin, -ray_origin) - t_ca * t_ca));
    if (d > radius)
        return infinity;

    highp float t_hc = sqrt(max(0.0, radius*radius - d*d));
    if (t_hc > t_ca) // inside
        return t_ca + t_hc;
    return t_ca - t_hc; // outside
    highp float t0 = t_ca - t_hc;
    if (t0 > 0)
        return t0;
    highp float t1 = t_ca + t_hc;
    if (t1 > 0)
        return t1;
    return infinity;
}

void main() {
   highp vec3 origin = vec3(camera_position);
   highp vec3 ray_direction = unproject(texcoords * 2.0 - 1.0);
   highp float ray_length = ray_sphere_intersect(vec2(0, origin.z/1000.0),
                                                 vec2(sqrt(max(0.0, 1 - ray_direction.z*ray_direction.z)), ray_direction.z),
                                                 earth_radius + atmosphere_height) * .1;
   highp vec3 light_through_atmosphere = lu_calculate_atmospheric_light(camera_position / 1000.0, ray_direction, ray_length, vec3(0.0, 0.0, 0.0), lookup_sampler);
//   highp vec3 light_through_atmosphere = rm_calculate_atmospheric_light(camera_position / 1000.0, ray_direction, ray_length, vec3(0.0, 0.0, 0.0));

   out_Color = vec4(light_through_atmosphere, 1.0);
//   highp float v = texture(lookup_sampler, ray_direction.xz).r;
//   float v = ray_direction.y;
//   out_Color = vec4(ray_direction, 1.0);
}

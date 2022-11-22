in highp vec2 texcoords;
out highp float out_Color;

const int n_optical_depth_steps = 100;
const highp float earth_radius = 6371.0;
const highp float infinity = 1.0 / 0.0;

highp float density_at_height(highp float height) {
   return exp(-height * 0.13);
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


// based on https://www.youtube.com/watch?v=DxfEbulyFcY
highp float rm_optical_depth(highp float height_origin, highp float ray_dir_cos_up) {
    highp vec2 ray_origin = vec2(0, earth_radius + height_origin);
    highp vec2 ray_direction = vec2(sqrt(max(0.0, 1 - ray_dir_cos_up*ray_dir_cos_up)), ray_dir_cos_up);
    highp float ray_length1 = ray_sphere_intersect(ray_origin, ray_direction, earth_radius - 20);
    highp float ray_length2 = ray_sphere_intersect(ray_origin, ray_direction, earth_radius + 100);
//    highp float ray_length = min(ray_sphere_intersect(ray_origin, ray_direction, earth_radius),
//                                 ray_sphere_intersect(ray_origin, ray_direction, earth_radius + 100));
    highp float ray_length = min(ray_length1, ray_length2);
//    return ray_length;//min(1000, ray_sphere_intersect(ray_origin, ray_direction, earth_radius + 100));

    highp vec2 density_sample_point = ray_origin;

    float step_size = ray_length / (n_optical_depth_steps - 1);
    float optical_depth = 0.0;
    for (int i = 0; i < n_optical_depth_steps; i++) {
        float height = length(density_sample_point) - earth_radius;
        float local_density = density_at_height(height);
        optical_depth += local_density;
        density_sample_point += ray_direction * step_size;
    }
    return optical_depth * (ray_length / n_optical_depth_steps);
}


void main() {
    highp float height = texcoords.x * 100;
//    highp float height = exp(texcoords.x * (log(100.0) - log(0.01)) + log(0.01)); // log scale
    highp float cos_up = texcoords.y * 2.0 - 1.0;
//    cos_up = max(0, cos_up);
    out_Color = rm_optical_depth(height, cos_up);
}

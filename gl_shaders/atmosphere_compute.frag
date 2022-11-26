in highp vec2 texcoords;
out highp float out_Color;

const int n_optical_depth_steps = 1000;
const highp float earth_radius = 6371.0;
const highp float atmosphere_height = 100.0;
const highp float infinity = 1.0 / 0.0;

highp float density_at_height(highp float height) {
    if (height < 0)
        return height = height * 0.0001;
//    if (height < -1)
//        return height = (height + 1.0) * 0.0001 - 1.0;
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
    highp float ray_length = ray_sphere_intersect(ray_origin, ray_direction, earth_radius + atmosphere_height);

    highp vec2 density_sample_point = ray_origin;

    float optical_depth = 0.0;
    for (float t = 0; t < ray_length;) {
        float height1 = length(density_sample_point) - earth_radius;
        float step_length = min(max(0.5, height1/10), ray_length - t);
        t += step_length;
        density_sample_point = ray_origin + t * ray_direction;
        float height2 = length(density_sample_point) - earth_radius;

        optical_depth += step_length * 0.5 * (density_at_height(height1) + density_at_height(height2));
    }
    return optical_depth;
}

void main() {
    highp float height = texcoords.x * atmosphere_height;
//    highp float height = exp(texcoords.x * (log(atmosphere_height) - log(0.01)) + log(0.01)); // log scale
    highp float cos_up = texcoords.y * 2.0 - 1.0;
//    cos_up = max(0, cos_up);
    out_Color = rm_optical_depth(height, cos_up);
}

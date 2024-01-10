
#define INF 1e9

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Sphere {
    vec3 position;
    float radius;
};

// quelle?
float intersect(Ray r, Sphere s) {
    vec3 op = s.position - r.origin;
    float eps = 0.001;
    float b = dot(op, r.direction);
    float det = b * b - dot(op, op) + s.radius * s.radius;
    if (det < 0.0)
        return INF;

    det = sqrt(det);
    float t1 = b - det;
    if (t1 > eps)
        return t1;

    float t2 = b + det;
    if (t2 > eps)
        return t2;

    return INF;
}

#define INF 1e9

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Sphere {
    vec3 position;
    float radius;
};

// a cylinder is defined by two points, p and q, and a radius
struct Cylinder {
    vec3 p, q;
    float radius;
};

float intersect_cylinder(Ray r, Cylinder) {
    return INF;
}

// Christer Ericson: Real-Time Collision Detection, Page 179
bool IntersectRaySphere(in Ray ray, in Sphere s, inout float t, inout vec3 q){
    vec3 d = ray.direction;
    vec3 p = ray.origin;
    vec3 m = p - s.position;

    float b = dot(m, d);
    float c = dot(m, m) - s.radius * s.radius;
    // Exit if r’s origin outside s (c > 0) and r pointing away from s (b > 0)
    if (c > 0 && b > 0) return false;
    float discr = b*b - c;

    // A negative discriminant corresponds to ray missing sphere
    if (discr < 0) return false;

    // Ray now found to intersect sphere, compute smallest t value of intersection
    t = -b - sqrt(discr);

    // If t is negative, ray started inside sphere so clamp t to zero
    if (t < 0) t = 0;

    q = ray.origin + ray.direction * t;
    return true;
}

// Christer Ericson: Real-Time Collision Detection, Page 197
// IntersectSegmentCylinder
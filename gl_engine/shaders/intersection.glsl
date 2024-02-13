
#define INF 1e9

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Sphere {
    vec3 position;
    float radius;
};

// a Capsule is defined by two points, p and q, and a radius
struct Capsule {
    vec3 p;
    vec3 q;
    float radius;
};

struct Plane {
    vec3 normal;
    float distance;
};

// quelle?
float intersect_sphere(Ray r, Sphere s) {
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

// https://www.shadertoy.com/view/Xt3SzX
vec3 capsule_normal( in vec3 pos, in vec3 a, in vec3 b, in float r ) {
    vec3  ba = b - a;
    vec3  pa = pos - a;
    float h = clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0);
    return (pa - h*ba)/r;
}

vec3 capsule_normal_2( in vec3 pos, in Capsule c) {
    return capsule_normal(pos, c.p, c.q, c.radius);
}

float signed_distance(in vec3 point, in Plane plane) {
    return dot(plane.normal, point) + plane.distance;
}

// https://www.shadertoy.com/view/Xt3SzX
// https://iquilezles.org/articles/intersectors/
float intersect_capsule( in vec3 ro, in vec3 rd, in vec3 pa, in vec3 pb, in float ra ) {
    vec3  ba = pb - pa;
    vec3  oa = ro - pa;
    float baba = dot(ba,ba);
    float bard = dot(ba,rd);
    float baoa = dot(ba,oa);
    float rdoa = dot(rd,oa);
    float oaoa = dot(oa,oa);
    float a = baba      - bard*bard;
    float b = baba*rdoa - baoa*bard;
    float c = baba*oaoa - baoa*baoa - ra*ra*baba;
    float h = b*b - a*c;
    if( h >= 0.0 )
    {
        float t = (-b-sqrt(h))/a;
        float y = baoa + t*bard;
        // body
        if( y>0.0 && y<baba ) return t;
        // caps
        vec3 oc = (y <= 0.0) ? oa : ro - pb;
        b = dot(rd,oc);
        c = dot(oc,oc) - ra*ra;
        h = b*b - c;
        if( h>0.0 ) return -b - sqrt(h);
    }
    return -1.0;
}

float intersect_capsule_2(in Ray r, in Capsule c) {
    return intersect_capsule(r.origin, r.direction, c.p, c.q, c.radius);
}
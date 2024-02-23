
#define INF 1e9

struct Ray {
    highp vec3 origin;
    highp vec3 direction;
};

struct Sphere {
    highp vec3 position;
    highp float radius;
};

// a Capsule is defined by two points, p and q, and a radius
struct Capsule {
    highp vec3 p;
    highp vec3 q;
    highp float radius;
};

struct Plane {
    highp vec3 normal;
    highp float distance;
};

// quelle?
highp float intersect_sphere(Ray r, Sphere s) {
    highp vec3 op = s.position - r.origin;
    highp float eps = 0.001;
    highp float b = dot(op, r.direction);
    highp float det = b * b - dot(op, op) + s.radius * s.radius;
    if (det < 0.0)
        return INF;

    det = sqrt(det);
    highp float t1 = b - det;
    if (t1 > eps)
        return t1;

    highp float t2 = b + det;
    if (t2 > eps)
        return t2;
}

// Christer Ericson: Real-Time Collision Detection, Page 179
bool IntersectRaySphere(in Ray ray, in Sphere s, inout highp float t, inout highp vec3 q){
    highp vec3 d = ray.direction;
    highp vec3 p = ray.origin;
    highp vec3 m = p - s.position;

    highp float b = dot(m, d);
    highp float c = dot(m, m) - s.radius * s.radius;

    // Exit if r’s origin outside s (c > 0) and r pointing away from s (b > 0)
    if (c > 0.0 && b > 0.0) return false;

    highp float discr = b*b - c;

    // A negative discriminant corresponds to ray missing sphere
    if (discr < 0.0) return false;

    // Ray now found to intersect sphere, compute smallest t value of intersection
    t = -b - sqrt(discr);

    // If t is negative, ray started inside sphere so clamp t to zero
    if (t < 0.0) t = 0.0;

    q = ray.origin + ray.direction * t;
    return true;
}

// https://www.shadertoy.com/view/Xt3SzX
highp vec3 capsule_normal( in highp vec3 pos, in highp vec3 a, in highp vec3 b, in highp float r ) {
    highp vec3  ba = b - a;
    highp vec3  pa = pos - a;
    highp float h = clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0);
    return (pa - h*ba)/r;
}

highp vec3 capsule_normal_2( in highp vec3 pos, in Capsule c) {
    return capsule_normal(pos, c.p, c.q, c.radius);
}

highp float signed_distance(in highp vec3 point, in Plane plane) {
    return dot(plane.normal, point) - plane.distance;
}

// https://www.shadertoy.com/view/Xt3SzX
// https://iquilezles.org/articles/intersectors/
highp float intersect_capsule( in highp vec3 ro, in highp vec3 rd, in highp vec3 pa, in highp vec3 pb, in highp float ra ) {
    highp vec3  ba = pb - pa;
    highp vec3  oa = ro - pa;
    highp float baba = dot(ba,ba);
    highp float bard = dot(ba,rd);
    highp float baoa = dot(ba,oa);
    highp float rdoa = dot(rd,oa);
    highp float oaoa = dot(oa,oa);
    highp float a = baba      - bard*bard;
    highp float b = baba*rdoa - baoa*bard;
    highp float c = baba*oaoa - baoa*baoa - ra*ra*baba;
    highp float h = b*b - a*c;
    if( h >= 0.0 )
    {
        highp float t = (-b-sqrt(h))/a;
        highp float y = baoa + t*bard;
        // body
        if( y>0.0 && y<baba ) return t;
        // caps
        highp vec3 oc = (y <= 0.0) ? oa : ro - pb;
        b = dot(rd,oc);
        c = dot(oc,oc) - ra*ra;
        h = b*b - c;
        if( h>0.0 ) return -b - sqrt(h);
    }
    return -1.0;
}

highp float intersect_capsule_2(in Ray r, in Capsule c) {
    return intersect_capsule(r.origin, r.direction, c.p, c.q, c.radius);
}
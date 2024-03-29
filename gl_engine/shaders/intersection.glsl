/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2024 Jakob Maier
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

#define INF 1e9

struct Ray {
    highp vec3 origin;
    highp vec3 direction;
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
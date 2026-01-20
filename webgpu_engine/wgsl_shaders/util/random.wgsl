/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

// Inspired by Path Tracer Compute Toy @https://compute.toys/view/1525

var<private> m_seed: vec4u;

fn seed(seed: vec4u) { m_seed = seed; }

fn PCG(x: vec4u) -> vec4u
{
    var v = x;
    
    v = v * vec4(1664525) + vec4(1013904223);
    
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    
    v ^= v >> vec4(16);
    
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    
    return v;
}

fn rand4() -> vec4f
{
    m_seed = PCG(m_seed);
    return vec4f(m_seed) / exp2(32);
}

fn rand() -> f32 { return rand4().x; }

fn rand2() -> vec2f { return rand4().xy; }

fn rand3() -> vec3f { return rand4().xyz; }
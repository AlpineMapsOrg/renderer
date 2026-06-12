/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Gerald Kimmersdorfer
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Patrick Komon
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

fn mod289_f32(x: f32) -> f32 { return x - floor(x * (1.0 / 289.0)) * 289.0; }
fn mod289_vec3f(x: vec3f) -> vec3f { return x - floor(x * (1.0 / 289.0)) * 289.0; }
fn mod289_vec4f(x: vec4f) -> vec4f { return x - floor(x * (1.0 / 289.0)) * 289.0; }
fn perm(x: vec4f) -> vec4f { return mod289_vec4f(((x * 34.0) + 1.0) * x); }

fn noise(p: vec3f) -> f32 {
    let p1 = mod289_vec3f(p);
    let a = floor(p1);
    var d = p1 - a;
    d = d * d * (3.0 - 2.0 * d);
    let b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    let k1 = perm(b.xyxy);
    let k2 = perm(k1.xyxy + b.zzww);
    let c = k2 + a.zzzz;
    let k3 = perm(c);
    let k4 = perm(c + 1.0);
    let o1 = fract(k3 * (1.0 / 41.0));
    let o2 = fract(k4 * (1.0 / 41.0));
    let o3 = o2 * d.z + o1 * (1.0 - d.z);
    let o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);
    return o4.y * d.y + o4.x * (1.0 - d.y);
}

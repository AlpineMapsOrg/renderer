/*****************************************************************************
* Alpine Renderer
* Copyright (C) 2022 Adam Celarek
* Copyright (C) 2023 Gerald Kimmersdorfer
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

fn octWrap(v: vec2<f32>) -> vec2<f32> {
    return (1.0 - abs(v.yx)) * (select(-1.0, 1.0, all(v >= vec2<f32>(0.0, 0.0))));
}

fn octNormalEncode2u16(normal: vec3<f32>) -> vec2<u32> {
    var n = normal;
    let d = abs(n.x) + abs(n.y) + abs(n.z);
    n /= d;
    var w = n.xy;
    if (n.z < 0.0) {
        w = octWrap(n.xy);
    }
    w = w * 0.5 + 0.5;
    return vec2<u32>(w * 65535.0);
}

fn octNormalDecode2u16(octNormal16: vec2<u32>) -> vec3<f32> {
    let f: vec2<f32> = vec2<f32>(octNormal16) / 65535.0 * 2.0 - 1.0;
    var n: vec3<f32> = vec3<f32>(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    let t: f32 = clamp(-n.z, 0.0, 1.0);
    if (n.x >= 0.0 && n.y >= 0.0) {
        n.x -= t;
        n.y -= t;
    } else {
        n.x += t;
        n.y += t;
    }
    return normalize(n);
}

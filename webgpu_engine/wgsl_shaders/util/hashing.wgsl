/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

fn compute_hash(a: u32) -> u32
{
   var b = (a + 2127912214u) + (a << 12u);
   b = (b ^ 3345072700u) ^ (b >> 19u);
   b = (b + 374761393u) + (b << 5u);
   b = (b + 3551683692u) ^ (b << 9u);
   b = (b + 4251993797u) + (b << 3u);
   b = (b ^ 3042660105u) ^ (b >> 16u);
   return b;
}

fn color_from_id_hash(a: u32) -> vec3f {
    var hash = compute_hash(a);
    return vec3f(f32(hash & 255u), f32((hash >> 8u) & 255u), f32((hash >> 16u) & 255u)) / 255.0;
}

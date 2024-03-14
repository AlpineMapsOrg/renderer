/*****************************************************************************
 * weBIGeo
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

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4f
{
    const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
    return vec4f(pos[vertex_index], 0, 1);
}

@fragment
fn fragmentMain() -> @location(0) vec4f
{
    return vec4f(0.0, 0.4, 1.0, 1.0);
}

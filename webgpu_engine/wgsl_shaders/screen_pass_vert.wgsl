/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2022 Adam Celarek
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

struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) texcoords : vec2f
}

@vertex
fn vertexMain(@builtin(vertex_index) vertex_index : u32) -> VertexOut {
    const VERTICES = array(
        vec2f(-1.0, -1.0),
        vec2f(3.0, -1.0),
        vec2f(-1.0, 3.0)
    );

    var vertex_out : VertexOut;
    vertex_out.position = vec4(VERTICES[vertex_index], 0.0, 1.0);
    vertex_out.texcoords = vec2(0.5, -0.5) * vertex_out.position.xy + vec2(0.5);
    return vertex_out;
}
/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek
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

// https://stackoverflow.com/a/59739538
out highp vec2 texcoords; // texcoords are in the normalized [0,1] range for the viewport-filling quad part of the triangle
void main() {
    vec2 vertices[3]=vec2[3](vec2(-1.0, -1.0),
                             vec2(3.0, -1.0),
                             vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 1.0, 1.0);
    texcoords = 0.5 * gl_Position.xy + vec2(0.5);
}

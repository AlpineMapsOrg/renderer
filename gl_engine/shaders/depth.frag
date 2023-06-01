/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Jakob Lindner
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

uniform highp vec3 camera_position;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

void main() {
   highp float dist = length(pos_wrt_cam);
   highp float depth = log(dist)/13.0;
   out_Color = vec4(encode(depth), 0, 0);
}

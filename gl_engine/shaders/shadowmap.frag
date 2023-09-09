/*****************************************************************************
 * Alpine Terrain Renderer
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

layout (location = 0) out highp float texout_depth;

in highp vec3 var_pos_wrt_cam;

//---------
in lowp vec2 uv;
uniform sampler2D texture_sampler;
//---------

void main() {
    texout_depth = gl_FragCoord.z;
    /*vec3 tmp = texture2D(texture_sampler, uv).rgb;
    texout_depth = ( tmp.x + tmp.y + tmp.z ) / 3.0;*/
}

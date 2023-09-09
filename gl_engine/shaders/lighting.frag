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

in highp vec2 texcoords;

uniform sampler2D tex_Albedo;
uniform sampler2D tex_Depth;
uniform sampler2D tex_Normal;

layout (location = 0) out lowp vec4 out_Color;

void main() {
    vec3 albedo = texture(tex_Albedo, texcoords).rgb;
    vec3 normal = texture(tex_Normal, texcoords).rgb;
    vec2 depth_encoded = texture(tex_Depth, texcoords).xy;

    out_Color = vec4(depth_encoded, 0.0, 1.0);
    if (texcoords.x < 0.6) out_Color = vec4(normal, 1.0);
    if (texcoords.x < 0.3) out_Color = vec4(albedo, 1.0);

    out_Color = vec4(albedo, 1.0);



}

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
uniform sampler2D font_sampler;
uniform sampler2D icon_sampler;

in highp vec2 texcoords;

layout (location = 0) out lowp vec4 out_Color;
vec3 fontColor = vec3(0.1);
vec3 outlineColor = vec3(0.9);


void main() {
    if(texcoords.x < 2.0)
    {
        float outline_mask = texture2D(font_sampler, texcoords).g;
        float font_mask = texture2D(font_sampler, texcoords).r;

        out_Color = vec4(mix(outlineColor, fontColor, font_mask), outline_mask);
    }
    else
    {
        out_Color = texture2D(icon_sampler, texcoords-vec2(10.0,10.0));
    }

}

/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Gerald Kimmersdorfer
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

#include "shared_config.glsl"
#include "camera_config.glsl"

layout (location = 0) out highp float out_ssao;

in highp vec2 texcoords;

uniform sampler2D texin_ssao;

uniform int direction;  // direction of blur, 0 = horizontal, 1 = vertical

const float offset[3] = float[](0.0, 1.3846153846, 3.2307692308);
const float weight[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

// Efficient gaussian blur with linear sampling
// https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
void main()
{
    out_ssao = texture2D(texin_ssao, texcoords).x * weight[0];
    if (direction == 0) {
        float scale_fact = 1.0 / camera.viewport_size.x;
        for (int i = 1; i < 3; i++) {
            out_ssao += texture2D(texin_ssao, texcoords + vec2(0.0, offset[i]) * scale_fact).r * weight[i];
            out_ssao += texture2D(texin_ssao, texcoords - vec2(0.0, offset[i]) * scale_fact).r * weight[i];
        }
    } else {
        float scale_fact = 1.0 / camera.viewport_size.y;
        for (int i = 1; i < 3; i++) {
            out_ssao += texture2D(texin_ssao, texcoords + vec2(offset[i], 0.0) * scale_fact).r * weight[i];
            out_ssao += texture2D(texin_ssao, texcoords - vec2(offset[i], 0.0) * scale_fact).r * weight[i];
        }
    }
}


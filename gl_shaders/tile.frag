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

uniform highp vec3 camera_position;
uniform sampler2D texture_sampler;
in lowp vec2 uv;
in highp vec3 pos_wrt_cam;
out lowp vec4 out_Color;

highp vec3 calculate_atmospheric_light(highp vec3 ray_origin, highp vec3 ray_direction, highp float ray_length, highp vec3 original_colour, int n_numerical_integration_steps);

void main() {
   highp vec3 origin = vec3(camera_position);
   highp vec4 ortho = texture(texture_sampler, uv);
   highp float dist = length(pos_wrt_cam);
   highp vec3 ray_direction = pos_wrt_cam / dist;

   highp vec3 light_through_atmosphere = calculate_atmospheric_light(camera_position / 1000.0, ray_direction, dist / 1000.0, vec3(ortho), 10);
   out_Color = vec4(light_through_atmosphere, 1.0);
}

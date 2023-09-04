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

#include "atmosphere_implementation.glsl"
#include "camera_config.glsl"

#line 2
in lowp vec2 texcoords;
in highp vec3 pos_wrt_cam;
layout (location = 0) out lowp vec4 out_Color;

//const highp float infinity = 1.0 / 0.0;   // gives a warning on webassembly (and other angle based products)
const highp float infinity = 3.40282e+38;   // https://godbolt.org/z/9o9PdbGqW

highp vec3 unproject(highp vec2 normalised_device_coordinates) {
   highp vec4 unprojected = camera.inv_proj_matrix * vec4(normalised_device_coordinates, 1.0, 1.0);
   highp vec4 normalised_unprojected = unprojected / unprojected.w;
   
   return normalize(vec3(camera.inv_view_matrix * normalised_unprojected));
}

void main() {
   highp vec3 origin = camera.position.xyz;
   highp vec3 ray_direction = unproject(texcoords * 2.0 - 1.0);
   highp float ray_length = 2000.0;
   highp vec3 background_colour = vec3(0.0, 0.0, 0.0);
   if (ray_direction.z < 0.0) {
       ray_length = min(ray_length, -(origin.z * 0.001) / ray_direction.z);
   }
   highp vec3 light_through_atmosphere = calculate_atmospheric_light(origin / 1000.0, ray_direction, ray_length, background_colour, 1000);

   out_Color = vec4(light_through_atmosphere, 1.0);
}

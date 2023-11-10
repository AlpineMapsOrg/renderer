/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Gerald Kimmersdorfer
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

const highp int SHADOW_CASCADES = 4;             // HAS TO BE THE SAME AS IN ShadowMapping.h!!
//const int SHADOW_CASCADES_ALIGNED = 2;     // either 4,8,12,16,...

layout (std140) uniform shadow_config {
    highp mat4 light_space_view_proj_matrix[SHADOW_CASCADES];
    highp float cascade_planes[SHADOW_CASCADES + 1];
    highp vec2 shadowmap_size;
    highp vec2 buff;
} shadow;

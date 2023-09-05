/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek, Gerald Kimmersdorfer
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

layout (std140) uniform shared_config {
    vec4 sun_light;
    vec4 sun_light_dir;
    vec4 amb_light;
    vec4 material_color;
    vec4 material_light_response;
    vec4 curtain_settings;
    bool phong_enabled;
    uint wireframe_mode;
    uint normal_mode;
    uint debug_overlay;
    float debug_overlay_strength;
    bool ssao_enabled;
    uint ssao_kernel;
    bool ssao_range_check;
    float ssao_falloff_to_value;
    vec3 buff;
} conf;

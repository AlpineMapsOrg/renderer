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

//#define FARPLANE_DEPTH_VALUE 0.0  // cause of reverse Z
#define FARPLANE_DEPTH_VALUE 1.0

// NOTE: Previously (~v23.11) the curtain settings were exposed to the GUI.
#define CURTAIN_DEBUG_MODE 0            // 0 normal (show curtains), 1 highlight curtains, 2 only show curtains
#define CURTAIN_HEIGHT_MODE 1           // 0 fixed (reference = height of curtain), 1 depth dependent
#define CURTAIN_REFERENCE_HEIGHT 200.0

layout (std140) uniform shared_config {
    highp vec4 sun_light;
    highp vec4 sun_light_dir;
    highp vec4 amb_light;
    highp vec4 material_color;
    highp vec4 material_light_response;
    highp vec4 snow_settings_angle;
    highp vec4 snow_settings_alt;

    highp float overlay_strength;
    highp float ssao_falloff_to_value;
    highp float padf1;
    highp float padf2;

    highp uint phong_enabled;
    highp uint normal_mode;
    highp uint overlay_mode;
    highp uint overlay_postshading_enabled;

    highp uint ssao_enabled;
    highp uint ssao_kernel;
    highp uint ssao_range_check;
    highp uint ssao_blur_kernel_size;

    highp uint height_lines_enabled;
    highp uint csm_enabled;
    highp uint overlay_shadowmaps_enabled;
    highp uint padi1;
} conf;

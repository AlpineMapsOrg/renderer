/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

struct shared_config {
    sun_light: vec4f,
    sun_light_dir: vec4f,
    amb_light: vec4f,
    material_color: vec4f,
    material_light_response: vec4f,
    snow_settings_angle: vec4f,
    snow_settings_alt: vec4f,

    overlay_strength: f32,
    ssao_falloff_to_value: f32,
    atmosphere_enabled: u32,
    padf2: f32,

    phong_enabled: u32,
    normal_mode: u32,
    overlay_mode: u32,
    overlay_postshading_enabled: u32,

    ssao_enabled: u32,
    ssao_kernel: u32,
    ssao_range_check: u32,
    ssao_blur_kernel_size: u32,

    height_lines_enabled: u32,
    csm_enabled: u32,
    overlay_shadowmaps_enabled: u32,
    track_render_mode: u32,

    // primary interval, secondary interval, base size, base darkening
    height_lines_settings: vec4f,

}

;

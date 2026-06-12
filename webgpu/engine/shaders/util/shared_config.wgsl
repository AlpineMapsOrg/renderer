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
    atmosphere_enabled: u32,
    clouds_enabled: u32,
    shading_enabled: u32,
    normal_mode: u32,
    overlay_mode: u32,
    track_render_mode: u32,
    _padding0: u32,
    _padding1: u32,
}

;

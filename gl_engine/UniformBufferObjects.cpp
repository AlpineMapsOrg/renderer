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
#include "UniformBufferObjects.h"

namespace gl_engine {

// IMPORTANT: Also serialize padding!
QDataStream& operator<<(QDataStream& out, const uboSharedConfig& data){
    out
        << data.m_sun_light
        << data.m_sun_light_dir
        << data.m_amb_light
        << data.m_material_color
        << data.m_material_light_response
        << data.m_curtain_settings
        << data.m_overlay_strength
        << data.m_ssao_falloff_to_value
        << data.padf1
        << data.padf2
        << data.m_phong_enabled
        << data.m_wireframe_mode
        << data.m_normal_mode
        << data.m_overlay_mode
        << data.m_overlay_postshading_enabled
        << data.m_ssao_enabled
        << data.m_ssao_kernel
        << data.m_ssao_range_check
        << data.m_ssao_blur_kernel_size
        << data.m_height_lines_enabled
        << data.m_csm_enabled
        << data.m_overlay_shadowmaps_enabled;
    return out;
}

QDataStream& operator>>(QDataStream& in, uboSharedConfig& data) {
    in
        >> data.m_sun_light
        >> data.m_sun_light_dir
        >> data.m_amb_light
        >> data.m_material_color
        >> data.m_material_light_response
        >> data.m_curtain_settings
        >> data.m_overlay_strength
        >> data.m_ssao_falloff_to_value
        >> data.padf1
        >> data.padf2
        >> data.m_phong_enabled
        >> data.m_wireframe_mode
        >> data.m_normal_mode
        >> data.m_overlay_mode
        >> data.m_overlay_postshading_enabled
        >> data.m_ssao_enabled
        >> data.m_ssao_kernel
        >> data.m_ssao_range_check
        >> data.m_ssao_blur_kernel_size
        >> data.m_height_lines_enabled
        >> data.m_csm_enabled
        >> data.m_overlay_shadowmaps_enabled;
    return in;
}

QDataStream& operator<<(QDataStream& out, const uboCameraConfig& data) { return out; }
QDataStream& operator>>(QDataStream& in, uboCameraConfig& data) { return in; }
QDataStream& operator<<(QDataStream& out, const uboShadowConfig& data) { return out; }
QDataStream& operator>>(QDataStream& in, uboShadowConfig& data) { return in; }
QDataStream& operator<<(QDataStream& out, const uboTestConfig& data) {
    out
        << data.m_tv4
        << data.m_tf32
        << data.m_tu32;
    return out;
}
QDataStream& operator>>(QDataStream& in, uboTestConfig& data) {
    in
        >> data.m_tv4
        >> data.m_tf32
        >> data.m_tu32;
    return in;
}
}

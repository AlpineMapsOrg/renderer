/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2023 alpinemaps.org
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

QDataStream& operator<<(QDataStream& out, const uboSharedConfig& data){
    out
        << data.m_sun_light
        << data.m_sun_light_dir
        << data.m_amb_light
        << data.m_material_color
        << data.m_material_light_response
        << data.m_curtain_settings
        << data.m_debug_overlay_strength
        << data.m_ssao_falloff_to_value
        << data.m_phong_enabled
        << data.m_wireframe_mode
        << data.m_normal_mode
        << data.m_debug_overlay
        << data.m_ssao_enabled
        << data.m_ssao_kernel
        << data.m_ssao_range_check
        << data.m_ssao_blur_kernel_size
        << data.m_height_lines_enabled
        << data.m_csm_enabled
        << data.m_overlay_shadowmaps;
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
        >> data.m_debug_overlay_strength
        >> data.m_ssao_falloff_to_value
        >> data.m_phong_enabled
        >> data.m_wireframe_mode
        >> data.m_normal_mode
        >> data.m_debug_overlay
        >> data.m_ssao_enabled
        >> data.m_ssao_kernel
        >> data.m_ssao_range_check
        >> data.m_ssao_blur_kernel_size
        >> data.m_height_lines_enabled
        >> data.m_csm_enabled
        >> data.m_overlay_shadowmaps;
    return in;
}

QDataStream& operator<<(QDataStream& out, const uboCameraConfig& data) { return out; }
QDataStream& operator>>(QDataStream& in, uboCameraConfig& data) { return in; }
QDataStream& operator<<(QDataStream& out, const uboShadowConfig& data) { return out; }
QDataStream& operator>>(QDataStream& in, uboShadowConfig& data) { return in; }
QDataStream& operator<<(QDataStream& out, const uboTestConfig& data) { return out; }
QDataStream& operator>>(QDataStream& in, uboTestConfig& data) { return in; }
}

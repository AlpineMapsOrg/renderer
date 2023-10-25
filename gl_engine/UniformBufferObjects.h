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

#pragma once

#include <QObject>
#include <QVector4D>
#include <glm/glm.hpp>
#include "ShadowMapping.h"

// NOTE: BOOLEANS BEHAVE WEIRD! JUST DONT USE THEM AND STICK TO 32bit Formats!!
// STD140 ALIGNMENT! USE PADDING IF NECESSARY. EVERY BLOCK OF SAME TYPE MUST BE PADDED
// TO 16 BYTE! (QPropertys don't matter)
// ANOTHER TIP: Stay away from vec3... always...
namespace gl_engine {

struct uboSharedConfig {

    Q_GADGET
public:
    // rgb...Color, a...intensity
    QVector4D m_sun_light = QVector4D(1.0, 1.0, 1.0, 0.25);
    // The direction of the light/sun in WS (northwest lighting at 45 degrees)
    QVector4D m_sun_light_dir = QVector4D(1.0, -1.0, -1.0, 0.0).normalized();
    //QVector4D m_sun_pos = QVector4D(1.0, 1.0, 3000.0, 1.0);
    // rgb...Color, a...intensity
    QVector4D m_amb_light = QVector4D(1.0, 1.0, 1.0, 0.5);
    // rgb...Color of the phong-material, a...opacity of ortho picture
    QVector4D m_material_color = QVector4D(0.5, 0.5, 0.5, 1.0);
    // amb, diff, spec, shininess
    QVector4D m_material_light_response = QVector4D(1.5, 3.0, 0.0, 32.0);
    // mode (0=disabled, 1=normal, 2=highlight), height_mode, fixed_height, unused
    QVector4D m_curtain_settings = QVector4D(1.0, 0.0, 500.0, 0.0);

    GLfloat m_debug_overlay_strength = 0.5;
    GLfloat m_ssao_falloff_to_value = 0.5;
    GLfloat padf1 = 0.0;
    GLfloat padf2 = 0.0;

    GLuint m_phong_enabled = false;
    // 0...disabled, 1...with shading, 2...white
    GLuint m_wireframe_mode = 0;
    // 0...per fragment, 1...FDM
    GLuint m_normal_mode = 1;
    // 0...nothing, 1...ortho, 2...normals, 3...tiles, 4...zoomlevel, 5...vertex-ID, 6...vertex heightmap
    GLuint m_debug_overlay = 0;

    GLuint m_ssao_enabled = false;
    GLuint m_ssao_kernel = 32;
    GLuint m_ssao_range_check = true;
    GLuint m_ssao_blur_kernel_size = 1;

    GLuint m_height_lines_enabled = false;
    GLuint m_csm_enabled = false;
    GLuint m_overlay_shadowmaps = false;
    GLuint padu1;

    // WARNING: Don't move the following Q_PROPERTIES to the top, otherwise the MOC
    // will do weird things with the data alignment!!
    Q_PROPERTY(QVector4D sun_light MEMBER m_sun_light)
    Q_PROPERTY(QVector4D sun_light_dir MEMBER m_sun_light_dir)
    Q_PROPERTY(QVector4D amb_light MEMBER m_amb_light)
    Q_PROPERTY(QVector4D material_color MEMBER m_material_color)
    Q_PROPERTY(QVector4D material_light_response MEMBER m_material_light_response)
    Q_PROPERTY(QVector4D curtain_settings MEMBER m_curtain_settings)

    Q_PROPERTY(float debug_overlay_strength MEMBER m_debug_overlay_strength)
    Q_PROPERTY(float ssao_falloff_to_value MEMBER m_ssao_falloff_to_value)

    Q_PROPERTY(bool phong_enabled MEMBER m_phong_enabled)
    Q_PROPERTY(unsigned int wireframe_mode MEMBER m_wireframe_mode)
    Q_PROPERTY(unsigned int normal_mode MEMBER m_normal_mode)
    Q_PROPERTY(unsigned int debug_overlay MEMBER m_debug_overlay)

    Q_PROPERTY(bool ssao_enabled MEMBER m_ssao_enabled)
    Q_PROPERTY(unsigned int ssao_kernel MEMBER m_ssao_kernel)
    Q_PROPERTY(bool ssao_range_check MEMBER m_ssao_range_check)
    Q_PROPERTY(unsigned int ssao_blur_kernel_size MEMBER m_ssao_blur_kernel_size)

    Q_PROPERTY(bool height_lines_enabled MEMBER m_height_lines_enabled)
    Q_PROPERTY(bool csm_enabled MEMBER m_csm_enabled)
    Q_PROPERTY(bool overlay_shadowmaps MEMBER m_overlay_shadowmaps)

    bool operator!=(const uboSharedConfig& rhs) const
    {
        // Note: It's possible to implement some compare function here, but in our case we just assume
        // that there are gonna be changes that need to be propagated from the UI to the render thread.
        // Since thats the only time this operator gets called (by the QT moc) we'll just return true.
        return true;
    }

};

// FOR SERIALIZATION
QDataStream& operator<<(QDataStream& out, const uboSharedConfig& data);
QDataStream& operator>>(QDataStream& in, uboSharedConfig& data);

struct uboCameraConfig {
    // Camera Position
    glm::vec4 position;
    // Camera View Matrix
    glm::mat4 view_matrix;
    // Camera Projection Matrix
    glm::mat4 proj_matrix;
    // Camera View-Projection Matrix
    glm::mat4 view_proj_matrix;
    // Camera Inverse View-Projection Matrix
    glm::mat4 inv_view_proj_matrix;
    // Camera Inverse View Matrix
    glm::mat4 inv_view_matrix;
    // Camera Inverse Projection Matrix
    glm::mat4 inv_proj_matrix;
    // Viewport Size in Pixel
    glm::vec2 viewport_size;
    glm::vec2 buffer2;

};

QDataStream& operator<<(QDataStream& out, const uboCameraConfig& data);
QDataStream& operator>>(QDataStream& in, uboCameraConfig& data);

struct uboShadowConfig {
    glm::mat4 light_space_view_proj_matrix[SHADOW_CASCADES];
    glm::vec4 cascade_planes[SHADOW_CASCADES + 1];  // vec4 necessary because of alignment (only x will be used)
    glm::vec2 shadowmap_size;
    glm::vec2 buff;
};

QDataStream& operator<<(QDataStream& out, const uboShadowConfig& data);
QDataStream& operator>>(QDataStream& in, uboShadowConfig& data);

// This struct is only used for unit tests
struct uboTestConfig {
    Q_GADGET
public:
    QVector4D m_tv4 = QVector4D(0.1, 0.2, 0.3, 0.4);
    GLfloat m_tf32 = 0.5f;
    GLuint m_tu32 = 201u;
    Q_PROPERTY(QVector4D tv4 MEMBER m_tv4)
    Q_PROPERTY(float tf32 MEMBER m_tf32)
    Q_PROPERTY(unsigned int tu32 MEMBER m_tu32)
    bool operator!=(const uboSharedConfig& rhs) const { return true; }
};

QDataStream& operator<<(QDataStream& out, const uboTestConfig& data);
QDataStream& operator>>(QDataStream& in, uboTestConfig& data);

}

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
#pragma once

#include <QObject>
#include <QVector4D>
#include <glm/glm.hpp>
#include "ShadowMapping.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

#include "nucleus/utils/UrlModifier.h"
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
    // rgba...Color of the phong-material (if a 0 -> ortho picture)
    QVector4D m_material_color = QVector4D(0.5, 0.5, 0.5, 0.0);
    // amb, diff, spec, shininess
    QVector4D m_material_light_response = QVector4D(1.5, 3.0, 0.0, 32.0);
    // mode (0=disabled, 1=normal, 2=highlight), height_mode, height_reference, unused
    QVector4D m_curtain_settings = QVector4D(1.0, 0.0, 150.0, 0.0);

    GLfloat m_overlay_strength = 0.5;
    GLfloat m_ssao_falloff_to_value = 0.5;
    GLfloat padf1 = 0.0;
    GLfloat padf2 = 0.0;

    GLuint m_phong_enabled = false;
    GLuint m_wireframe_mode = 0;                    // 0...disabled, 1...with shading, 2...white
    GLuint m_normal_mode = 1;                       // 0...per fragment, 1...FDM
    GLuint m_overlay_mode = 0;                      // see GlSettings.qml for list of modes

    GLuint m_overlay_postshading_enabled = false;   // see GlSettings.qml for more details
    GLuint m_ssao_enabled = false;
    GLuint m_ssao_kernel = 32;
    GLuint m_ssao_range_check = true;

    GLuint m_ssao_blur_kernel_size = 1;
    GLuint m_height_lines_enabled = false;
    GLuint m_csm_enabled = false;
    GLuint m_overlay_shadowmaps_enabled = false;

    // WARNING: Don't move the following Q_PROPERTIES to the top, otherwise the MOC
    // will do weird things with the data alignment!!
    Q_PROPERTY(QVector4D sun_light MEMBER m_sun_light)
    Q_PROPERTY(QVector4D sun_light_dir MEMBER m_sun_light_dir)
    Q_PROPERTY(QVector4D amb_light MEMBER m_amb_light)
    Q_PROPERTY(QVector4D material_color MEMBER m_material_color)
    Q_PROPERTY(QVector4D material_light_response MEMBER m_material_light_response)
    Q_PROPERTY(QVector4D curtain_settings MEMBER m_curtain_settings)

    Q_PROPERTY(float overlay_strength MEMBER m_overlay_strength)
    Q_PROPERTY(float ssao_falloff_to_value MEMBER m_ssao_falloff_to_value)

    Q_PROPERTY(bool phong_enabled MEMBER m_phong_enabled)
    Q_PROPERTY(unsigned int wireframe_mode MEMBER m_wireframe_mode)
    Q_PROPERTY(unsigned int normal_mode MEMBER m_normal_mode)
    Q_PROPERTY(unsigned int overlay_mode MEMBER m_overlay_mode)

    Q_PROPERTY(bool overlay_postshading_enabled MEMBER m_overlay_postshading_enabled)
    Q_PROPERTY(bool ssao_enabled MEMBER m_ssao_enabled)
    Q_PROPERTY(unsigned int ssao_kernel MEMBER m_ssao_kernel)
    Q_PROPERTY(bool ssao_range_check MEMBER m_ssao_range_check)

    Q_PROPERTY(unsigned int ssao_blur_kernel_size MEMBER m_ssao_blur_kernel_size)
    Q_PROPERTY(bool height_lines_enabled MEMBER m_height_lines_enabled)
    Q_PROPERTY(bool csm_enabled MEMBER m_csm_enabled)
    Q_PROPERTY(bool overlay_shadowmaps_enabled MEMBER m_overlay_shadowmaps_enabled)

    bool operator!=(const uboSharedConfig& rhs) const
    {
        // NOTE: I'll do a hack here! I know that our data needs to be vec4 aligned, therefore I
        // compare whole 64bit uints as this should be quite fast on the machine.
        int size_64bit_packages = sizeof(uboSharedConfig) / 8;
        assert(size_64bit_packages * 8 == sizeof(uboSharedConfig)); // make sure its really (at least) vec2 aligned
        const uint64_t* lhs_64 = reinterpret_cast<const uint64_t*>(this);
        const uint64_t* rhs_64 = reinterpret_cast<const uint64_t*>(&rhs);
        for (int i = 0; i < size_64bit_packages; i++) {
            if (lhs_64[i] != rhs_64[i]) return true;
        }
        // Note: If you don't like the above you can also just return true all the time!
        // We only need this comparison when configuration has changed anyway. That shouldn't
        // be that often.
        return false;
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



// Returns String representation of buffer data (Base64)
template <typename T>
QString ubo_as_string(T ubo) {
    QByteArray buffer;
    QDataStream out_stream(&buffer, QIODevice::WriteOnly);
    out_stream << ubo;
    auto compressedData = qCompress(buffer, 9);
    auto b64Data = compressedData.toBase64();
    auto b64String = QString(b64Data);
    auto b64StringUrlSafe = nucleus::utils::UrlModifier::b64_to_urlsafe_b64(b64String);
    return b64StringUrlSafe;
}

// Loads the given base 64 encoded string as the buffer data
template <typename T>
T ubo_from_string(const QString& base64StringUrlSafe, bool* successful = nullptr) {
    T ubo;
    if (successful) *successful = true;
    auto b64String = nucleus::utils::UrlModifier::urlsafe_b64_to_b64(base64StringUrlSafe);
    QByteArray buffer = QByteArray::fromBase64(b64String.toUtf8());
    buffer = qUncompress(buffer);
    if (buffer.size() < sizeof(T)) {
        qWarning() << "Data in given base64string too short for type size. Standard Type will be returned...";
        if (successful) *successful = false;
        return ubo;
    }
    QDataStream inStream(&buffer, QIODevice::ReadOnly);
    inStream >> ubo;
    return ubo;
}

}

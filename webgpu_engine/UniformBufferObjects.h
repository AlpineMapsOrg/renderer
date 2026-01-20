/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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
#include <glm/glm.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

#include "nucleus/utils/UrlModifier.h"

// NOTE: The following constant gets serialized at the beginning of the data. This way
//      we can adapt the deserializing method to also support links that were created
//      in older versions of the Alpine Maps APP. That means:
// IMPORTANT: Whenever changes on the shared config are done and merged upstream (such that it replaces
//      the current instance on alpinemaps.org) this version number needs to be raised and the deserializing
//      method needs to be adapted to work in a backwards compatible fashion!
//      NOTE: THIS FUNCTIONALITY WAS NOT IN PLACE FOR VERSION 1. Those links therefore (in the best case) don't work anymore.
#define CURRENT_UBO_VERSION 2

// NOTE: BOOLEANS BEHAVE WEIRD! JUST DONT USE THEM AND STICK TO 32bit Formats!!
// STD140 ALIGNMENT! USE PADDING IF NECESSARY. EVERY BLOCK OF SAME TYPE MUST BE PADDED
// TO 16 BYTE! (QPropertys don't matter)
// ANOTHER TIP: Stay away from vec3... always...
namespace webgpu_engine {

struct uboSharedConfig {
public:
    // rgb...Color, a...intensity
    glm::vec4 m_sun_light = glm::vec4(1.0, 1.0, 1.0, 0.2);
    // The direction of the light/sun in WS (northwest lighting at 45 degrees)
    glm::vec4 m_sun_light_dir = glm::normalize(glm::vec4(1.0, -1.0, -1.0, 0.0));
    //glm::vec4 m_sun_pos = glm::vec4(1.0, 1.0, 3000.0, 1.0);
    // rgb...Color, a...intensity
    glm::vec4 m_amb_light = glm::vec4(1.0, 1.0, 1.0, 0.5);
    // rgba...Color of the phong-material (if a 0 -> ortho picture)
    glm::vec4 m_material_color = glm::vec4(1.0, 1.0, 1.0, 0.0);
    // amb, diff, spec, shininess
    glm::vec4 m_material_light_response = glm::vec4(1.5, 3.0, 0.0, 32.0);
    // enabled, min angle, max angle, angle blend space
    glm::vec4 m_snow_settings_angle = glm::vec4(0.0, 0.0, 45.0, 5.0);
    // min altitude (snowline), variating altitude, altitude blend space, spec addition
    glm::vec4 m_snow_settings_alt = glm::vec4(1000.0, 200.0, 200.0, 1.0);

    float m_overlay_strength = 1.0f;
    float m_ssao_falloff_to_value = 0.5f;
    uint32_t m_atmosphere_enabled = true;
    float padf2 = 0.0f;

    uint32_t m_phong_enabled = true;
    uint32_t m_normal_mode = 2; // 0...none, 1...per fragment, 2...FDM
    uint32_t m_overlay_mode = 0; // see GlSettings.qml for list of modes
    uint32_t m_overlay_postshading_enabled = false;   // see GlSettings.qml for more details

    uint32_t m_ssao_enabled = false;
    uint32_t m_ssao_kernel = 32;
    uint32_t m_ssao_range_check = true;
    uint32_t m_ssao_blur_kernel_size = 1;

    uint32_t m_height_lines_enabled = false;
    uint32_t m_csm_enabled = false;
    uint32_t m_overlay_shadowmaps_enabled = false;
    uint32_t m_track_render_mode = 0; // 0...none, 1...without depth test, 2...with depth test, 3 semi-transparent if behind terrain

    glm::vec4 m_height_lines_settings = glm::vec4(250.0f, 50.0f, 2.0, 0.3); // primary interval, secondary interval, base size, base darkening
};

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
    // the distance scaling factor of the camera
    float distance_scaling_factor;
    float buffer2;
};

// contains settings (including world space aabb) for image overlay (used in compose step)
struct ImageOverlaySettings {
    glm::vec2 aabb_min = { 0.0f, 0.0f };
    glm::vec2 aabb_max = { 0.0f, 0.0f };

    float alpha = 1.0f;
    uint32_t mode = 1;
    float float_decoding_lower_bound = 0.0f;
    float float_decoding_upper_bound = 20.0f;

    glm::vec2 texture_size = { 0.0f, 0.0f };
    glm::vec2 padding;
};

//TODO
/*
struct uboShadowConfig {
    glm::mat4 light_space_view_proj_matrix[SHADOW_CASCADES];
    glm::vec4 cascade_planes[SHADOW_CASCADES + 1];  // vec4 necessary because of alignment (only x will be used)
    glm::vec2 shadowmap_size;
    glm::vec2 buff;
};
*/

// FOR SERIALIZATION
// NOTE: The following are the default serialize functions. (nothing happens) If the functionality is needed
// have a look at how you have to override those functions for the specific ubos.
template <typename T>
void serialize_ubo(QDataStream& /*out*/, const T& /*data*/) { }
template <typename T>
void unserialize_ubo(QDataStream& /*in*/, T& /*data*/, uint32_t /*version*/) { }

// override for shared_config
//void serialize_ubo(QDataStream& out, const uboSharedConfig& data);
//void unserialize_ubo(QDataStream& in, uboSharedConfig& data, uint32_t version);

// Returns String representation of buffer data (Base64)
template <typename T>
QString ubo_as_string(T ubo) {
    QByteArray buffer;
    QDataStream out_stream(&buffer, QIODevice::WriteOnly);
    out_stream << (uint32_t)CURRENT_UBO_VERSION; // serialize version number first
    serialize_ubo(out_stream, ubo);
    auto compressedData = qCompress(buffer, 9);
    auto b64Data = compressedData.toBase64();
    auto b64String = QString(b64Data);
    auto b64StringUrlSafe = nucleus::utils::UrlModifier::b64_to_urlsafe_b64(b64String);
    return b64StringUrlSafe;
}

// Loads the given base 64 encoded string as the buffer data
template <typename T>
T ubo_from_string(const QString& base64StringUrlSafe, bool* successful = nullptr) {
    T ubo {};
    if (successful) *successful = true;
    auto b64String = nucleus::utils::UrlModifier::urlsafe_b64_to_b64(base64StringUrlSafe);
    QByteArray buffer = QByteArray::fromBase64(b64String.toUtf8());
    buffer = qUncompress(buffer);
    // NOTE: buffer size is not equal to sizeof(T)! Qt also saves qt dependent version information.
    QDataStream inStream(&buffer, QIODevice::ReadOnly);
    uint32_t version;
    inStream >> version;
    if (version > CURRENT_UBO_VERSION || version == 0) {
        qWarning() << "UBO data string has an invalid version number (" << version << "). Default ubo will be used...";
        if (successful) *successful = false;
        return ubo;
    }
    unserialize_ubo(inStream, ubo, version);
    return ubo;
}

}

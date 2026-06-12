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

#include <cstdint>
#include <glm/glm.hpp>

// STD140 ALIGNMENT! USE PADDING IF NECESSARY. EVERY BLOCK OF SAME TYPE MUST BE PADDED TO 16 BYTE!
// Also: stay away from vec3 (alignment pitfalls), and don't use bool (use 32-bit types instead).
namespace webgpu_engine {

struct uboSharedConfig {
public:
    // rgb...Color, a...intensity
    glm::vec4 m_sun_light = glm::vec4(1.0, 1.0, 1.0, 0.2);
    // The direction of the light/sun in WS (northwest lighting at 45 degrees)
    glm::vec4 m_sun_light_dir = glm::normalize(glm::vec4(1.0, -1.0, -1.0, 0.0));
    // rgb...Color, a...intensity
    glm::vec4 m_amb_light = glm::vec4(1.0, 1.0, 1.0, 0.5);
    // rgba...Color of the phong-material (if a 0 -> ortho picture)
    glm::vec4 m_material_color = glm::vec4(1.0, 1.0, 1.0, 0.0);
    // amb, diff, spec, shininess
    glm::vec4 m_material_light_response = glm::vec4(1.5, 3.0, 0.0, 32.0);

    uint32_t m_atmosphere_enabled = true;
    uint32_t m_clouds_enabled = true;
    uint32_t m_shading_enabled = true;
    uint32_t m_normal_mode = 2; // 0...none, 1...per fragment, 2...FDM

    uint32_t m_overlay_mode = 0; // per-tile debug data packed into GBuffer slot 3 (see TileDebugOverlay)
    uint32_t m_track_render_mode = 1; // 0...none, 1...without depth test, 2...with depth test, 3 semi-transparent if behind terrain
    uint32_t m_padding0 = 0; // std140: pad the trailing scalar block to a 16-byte boundary
    uint32_t m_padding1 = 0;
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

} // namespace webgpu_engine

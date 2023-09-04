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

namespace gl_engine {

    struct uboSharedConfig {

        Q_GADGET
            public:
        // rgb...Color, a...intensity
        QVector4D m_sun_light = QVector4D(1.0, 1.0, 1.0, 0.5);
        // The direction of the light/sun in WS (northwest lighting at 45 degrees)
        QVector4D m_sun_light_dir = QVector4D(1.0, -1.0, -1.0, 0.0).normalized();
        // rgb...Color, a...intensity
        QVector4D m_amb_light = QVector4D(1.0, 1.0, 1.0, 0.5);
        // rgb...Color of the phong-material, a...opacity of ortho picture
        QVector4D m_material_color = QVector4D(0.5, 0.5, 0.5, 0.5);
        // amb, diff, spec, shininess
        QVector4D m_material_light_response = QVector4D(1.5, 3.0, 0.0, 32.0);
        // mode (0=disabled, 1=normal, 2=highlight), height_mode, fixed_height, unused
        QVector4D m_curtain_settings = QVector4D(1.0, 0.0, 500.0, 0.0);

        bool m_phong_enabled = true;
        // 0...disabled, 1...with shading, 2...white
        unsigned int m_wireframe_mode = 0;
        // 0...per fragment, 1...FDM
        unsigned int m_normal_mode = 1;
        // 0...nothing, 1...ortho, 2...normals, 3...tiles, 4...zoomlevel, 5...vertex-ID, 6...vertex heightmap
        unsigned int m_debug_overlay = 0;
        float m_debug_overlay_strength = 0.5;
        glm::vec3 buffer3;

        Q_PROPERTY(QVector4D sun_light MEMBER m_sun_light)
        Q_PROPERTY(QVector4D sun_light_dir MEMBER m_sun_light_dir)
        Q_PROPERTY(QVector4D amb_light MEMBER m_amb_light)
        Q_PROPERTY(QVector4D material_color MEMBER m_material_color)
        Q_PROPERTY(QVector4D material_light_response MEMBER m_material_light_response)
        Q_PROPERTY(QVector4D curtain_settings MEMBER m_curtain_settings)
        Q_PROPERTY(bool phong_enabled MEMBER m_phong_enabled)
        Q_PROPERTY(unsigned int normal_mode MEMBER m_normal_mode)
        Q_PROPERTY(unsigned int debug_overlay MEMBER m_debug_overlay)
        Q_PROPERTY(float debug_overlay_strength MEMBER m_debug_overlay_strength)
        Q_PROPERTY(unsigned int wireframe_mode MEMBER m_wireframe_mode)



        bool operator!=(const uboSharedConfig& rhs) const
        {
            // Note: It's possible to implement some compare function here, but in our case we just assume
            // that there are gonna be changes that need to be propagated from the UI to the render thread.
            // Since thats the only time this operator gets called (by the QT moc) we'll just return true.
            return true;
        }

    };

    struct uboCameraConfig {

    public:
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

}

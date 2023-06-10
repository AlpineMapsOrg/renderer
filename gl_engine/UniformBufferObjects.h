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

namespace gl_engine {

    struct uboSharedConfig {
        Q_GADGET
        // rgb...Color, a...intensity
        QVector4D m_sun_light = QVector4D(1.0, 1.0, 1.0, 1.0);
        // The direction of the light/sun in WS (northwest lighting at 45 degrees)
        QVector4D m_sun_light_dir = QVector4D(1.0, -1.0, -1.0, 0.0);
        // rgb...Color, a...intensity
        QVector4D m_amb_light = QVector4D(1.0, 1.0, 1.0, 0.05);
        // rgb...Color of the phong-material, a...opacity of ortho picture
        QVector4D m_material_color = QVector4D(0.5, 0.5, 0.5, 0.5);
        // amb, diff, spec, shininess
        QVector4D m_material_light_response = QVector4D(1.5, 3.0, 0.5, 32.0);
        // 0...nothing, 1...normals, 2...tiles, 3...zoomlevel, 4...Triangles
        unsigned int m_debug_overlay = 0;
        float m_debug_overlay_strength = 0.5;

        Q_PROPERTY(QVector4D sun_light MEMBER m_sun_light)
        Q_PROPERTY(QVector4D sun_light_dir MEMBER m_sun_light_dir)
        Q_PROPERTY(QVector4D amb_light MEMBER m_amb_light)
        Q_PROPERTY(QVector4D material_color MEMBER m_material_color)
        Q_PROPERTY(QVector4D material_light_response MEMBER m_material_light_response)
        Q_PROPERTY(unsigned int debug_overlay MEMBER m_debug_overlay)
        Q_PROPERTY(float debug_overlay_strength MEMBER m_debug_overlay_strength)

    public:
        bool operator!=(const uboSharedConfig& rhs) const
        {
            // ToDo compare for difference
            return true;
        }

    };

}

/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include <QString>
#include <QVariant>
#include <Qt>

namespace nucleus::map_label {

struct MapLabel {
    enum class Role {
        Text = Qt::UserRole,
        Latitde,
        Longitude,
        Altitude,
        Importance,
        ViewportX,
        ViewportY,
        ViewportSize
    };

    QString text;
    double latitude;
    double longitude;
    float altitude;
    float importance;
    float viewport_x;
    float viewport_y;
    float viewport_size;

    QVariant get(Role r) const
    {
        switch (r) {
        case Role::Text:
            return text;
        case Role::Latitde:
            return latitude;
        case Role::Longitude:
            return longitude;
        case Role::Altitude:
            return altitude;
        case Role::Importance:
            return importance;
        case Role::ViewportX:
            return viewport_x;
        case Role::ViewportY:
            return viewport_y;
        case Role::ViewportSize:
            return viewport_size;
        }
        return {};
    }
};

}

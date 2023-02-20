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

#include "CameraTransformationProxyModel.h"

#include <QThread>

#include "app/MapLabel.h"
#include "nucleus/srs.h"

CameraTransformationProxyModel::CameraTransformationProxyModel()
{
}

QVariant CameraTransformationProxyModel::data(const QModelIndex& index, int role) const
{
    if (role != int(MapLabel::Role::ViewportX) && role != int(MapLabel::Role::ViewportY) && role != int(MapLabel::Role::ViewportSize))
        return QIdentityProxyModel::data(index, role);

    const double lat = QIdentityProxyModel::data(index, int(MapLabel::Role::Latitde)).toDouble();
    const double lon = QIdentityProxyModel::data(index, int(MapLabel::Role::Longitude)).toDouble();
    const double alt = QIdentityProxyModel::data(index, int(MapLabel::Role::Altitude)).toDouble();
    const auto world_xy = nucleus::srs::lat_long_to_world({ lat, lon });
    const auto world_pos = glm::dvec3(world_xy, alt);

    if (role == int(MapLabel::Role::ViewportSize)) {
        const auto distance = glm::distance(world_pos, m_camera.position());
        const auto size_scale = m_camera.to_screen_space(1, distance);
        return size_scale;
    }

    const auto projected = m_camera.local_view_projection_matrix(m_camera.position()) * glm::dvec4(world_pos - m_camera.position(), 1.0);
    if (projected.z < 0)
        return -10000; // should be outside of screen
    const auto ndc_pos = glm::fvec2(projected / projected.w);
    const auto viewport_pos = (ndc_pos * glm::fvec2(1, -1) * 0.5f + 0.5f) * glm::fvec2(m_camera.viewport_size());

    if (role == int(MapLabel::Role::ViewportX)) {
        return viewport_pos.x;
    }
    return viewport_pos.y;
}

QHash<int, QByteArray> CameraTransformationProxyModel::roleNames() const
{
    auto role_names = sourceModel()->roleNames();
    role_names.insert(int(MapLabel::Role::ViewportX), "x");
    role_names.insert(int(MapLabel::Role::ViewportY), "y");
    role_names.insert(int(MapLabel::Role::ViewportSize), "size");
    return role_names;
}

nucleus::camera::Definition CameraTransformationProxyModel::camera() const
{
    return m_camera;
}

void CameraTransformationProxyModel::set_camera(const nucleus::camera::Definition& new_camera)
{
    if (m_camera == new_camera)
        return;
    m_camera = new_camera;
    m_camera.set_near_plane(10);
    emit camera_changed();
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), { int(MapLabel::Role::ViewportX), int(MapLabel::Role::ViewportY), int(MapLabel::Role::ViewportSize) });
}

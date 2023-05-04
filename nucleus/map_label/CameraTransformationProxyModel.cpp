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

#include "MapLabel.h"
#include "nucleus/srs.h"

namespace nucleus::map_label {

CameraTransformationProxyModel::CameraTransformationProxyModel()
{
    connect(this, &QAbstractProxyModel::sourceModelChanged, this, &CameraTransformationProxyModel::source_data_updated);
}

QVariant CameraTransformationProxyModel::data(const QModelIndex& index, int role) const
{
    if (role != int(MapLabel::Role::ViewportX) && role != int(MapLabel::Role::ViewportY) && role != int(MapLabel::Role::ViewportSize))
        return QIdentityProxyModel::data(index, role);

    return m_labels[index.row()].get(MapLabel::Role(role));
}

std::vector<MapLabel> CameraTransformationProxyModel::data() const
{
    return m_labels;
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
    recalculate_screen_space_data();
    emit camera_changed();
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), { int(MapLabel::Role::ViewportX), int(MapLabel::Role::ViewportY), int(MapLabel::Role::ViewportSize) });
}

void CameraTransformationProxyModel::source_data_updated()
{
    const auto* source_model = dynamic_cast<AbstractMapLabelModel*>(sourceModel());
    assert(source_model != nullptr);
    m_labels = source_model->data();
    recalculate_screen_space_data();
}

void CameraTransformationProxyModel::recalculate_screen_space_data()
{
    for (auto& label : m_labels) {
        const auto world_pos = nucleus::srs::lat_long_alt_to_world({ label.latitude, label.longitude, label.altitude });

        const auto distance = glm::distance(world_pos, m_camera.position());
        label.viewport_size = m_camera.to_screen_space(1, distance);

        const auto projected = m_camera.local_view_projection_matrix(m_camera.position()) * glm::dvec4(world_pos - m_camera.position(), 1.0);
        const auto ndc_pos = glm::fvec2(projected / projected.w);
        const auto viewport_pos = (ndc_pos * glm::fvec2(1, -1) * 0.5f + 0.5f) * glm::fvec2(m_camera.viewport_size());
        label.viewport_x = projected.z >= 0 ? viewport_pos.x : -10000;
        label.viewport_y = projected.z >= 0 ? viewport_pos.y : -10000;
    }
}

}

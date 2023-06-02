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

#include "MapLabelModel.h"

#include <QThread>
#include <QTimer>

namespace nucleus::map_label {

MapLabelModel::MapLabelModel(QObject* parent)
    : QAbstractListModel { parent }
{
    //    QString text;
    //    double latitude;
    //    double longitude;
    //    float altitude;
    //    float importance;
    //    float viewport_x;
    //    float viewport_y;
    //    float viewport_size;
    m_labels.push_back({ "Ackerlspitze", 47.559125, 12.347188, 2329, 8, 0, 0, 0 });
    m_labels.push_back({ "Westliche Hochgrubachspitze", 47.5583658, 12.3433997, 2277, 6, 0, 0, 0 });
    m_labels.push_back({ "Östliche Hochgrubachspitze", 47.5587933, 12.3450985, 2284, 6, 0, 0, 0 });
    m_labels.push_back({ "Scheffauer", 47.5573214, 12.2418396, 2111, 8, 0, 0, 0 });
    m_labels.push_back({ "Maukspitze", 47.5588954, 12.3563668, 2231, 8, 0, 0, 0 });
    m_labels.push_back({ "Ellmauer Halt", 47.5616377, 12.3025296, 2342, 9, 0, 0, 0 });
}

int MapLabelModel::rowCount(const QModelIndex& parent) const
{
    return m_labels.size();
}

QVariant MapLabelModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (unsigned(index.row()) >= m_labels.size())
        return {};

    return m_labels[index.row()].get(MapLabel::Role(role));
}

std::vector<MapLabel> MapLabelModel::data() const
{
    return m_labels;
}

QHash<int, QByteArray> MapLabelModel::roleNames() const
{
    return {
        { int(MapLabel::Role::Text), "text" },
        { int(MapLabel::Role::Latitde), "latitude" },
        { int(MapLabel::Role::Longitude), "longitude" },
        { int(MapLabel::Role::Altitude), "altitude" },
        { int(MapLabel::Role::Importance), "importance" }
    };
}

}

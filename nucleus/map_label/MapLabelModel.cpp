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
    m_labels.push_back({ "Westliche Hochgrubachspitze", 47.5583658, 12.3433997, 2277, 4, 0, 0, 0 });
    m_labels.push_back({ "Östliche Hochgrubachspitze", 47.5587933, 12.3450985, 2284, 4, 0, 0, 0 });
    m_labels.push_back({ "Scheffauer", 47.5573214, 12.2418396, 2111, 8, 0, 0, 0 });
    m_labels.push_back({ "Maukspitze", 47.5588954, 12.3563668, 2231, 8, 0, 0, 0 });
    m_labels.push_back({ "Ellmauer Halt", 47.5616377, 12.3025296, 2342, 8, 0, 0, 0 });
    m_labels.push_back({ "Schönfeldspitze", 47.45831, 12.93774, 2653, 8, 0, 0});
    m_labels.push_back({ "Klosterwappen", 47.76706, 15.80450, 2076, 8, 0, 0});
    m_labels.push_back({ "Großglockner", 47.07455, 12.69388, 3798, 8, 0, 0});
    m_labels.push_back({ "Hochschwab", 47.61824, 15.14245, 2277, 8, 0, 0});

    // Schafberg
    m_labels.push_back({ "Schafberg", 47.77639, 13.43389, 1783, 8, 0, 0});
    m_labels.push_back({ "Schafbergbahn Tal", 47.73963, 13.43980, 542, 5, 0, 0});
    m_labels.push_back({ "Schafbergbahn Mitte", 47.76980, 13.42216, 1363, 5, 0, 0});
    m_labels.push_back({ "Schafbergbahn Berg", 47.77512, 13.43400, 1732, 5, 0, 0});
    m_labels.push_back({ "St. Wolfgang", 47.73787, 13.44827, 548, 6, 0, 0});
    m_labels.push_back({ "Hoher Dachstein", 47.47519, 13.60569, 2995, 8, 0, 0});
    m_labels.push_back({ "Brennerin", 47.81659, 13.57866, 1605, 8, 0, 0});
    m_labels.push_back({ "Drachenwand", 47.81377, 13.34725, 1176, 7, 0, 0});
    m_labels.push_back({ "Unterach", 47.80431, 13.48897, 477, 7, 0, 0});
    m_labels.push_back({ "Krottensee", 47.78318, 13.38849, 577, 7, 0, 0});

    // Ötschergräben
    m_labels.push_back({ "Ötscher", 47.86186, 15.20251, 1893, 8, 0, 0});
    m_labels.push_back({ "Kraftwerk Wienerbruck", 47.85239, 15.28817, 630, 6, 0, 0});
    m_labels.push_back({ "Mirafall", 47.84454, 15.24895, 770, 6, 0, 0});
    m_labels.push_back({ "Schleierfall", 47.84124, 15.21863, 800, 6, 0, 0});

    // Grubenkarspitze
    m_labels.push_back({ "Grubenkarspitze", 47.38078, 11.52211, 2663, 8, 0, 0});
    m_labels.push_back({ "Sunntigerspitze", 47.36470, 11.48137, 2322, 6, 0, 0});
    m_labels.push_back({ "Reps", 47.36612, 11.46206, 2160, 6, 0, 0});

    // Gimpel
    m_labels.push_back({ "Gimpel", 47.50127, 10.61249, 2176, 8, 0, 0});
    m_labels.push_back({ "Rote Flüh", 47.49962, 10.60855, 2111, 6, 0, 0});
    m_labels.push_back({ "Otto-Mayr-Hütte", 47.50934, 10.61872, 1530, 6, 0, 0});
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

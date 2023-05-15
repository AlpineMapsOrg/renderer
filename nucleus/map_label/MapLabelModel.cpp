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

    // Training Set
    m_labels.push_back({ "Ackerlspitze", 47.559125, 12.347188, 2329, 97, 0, 0, 0 });
    m_labels.push_back({ "Westliche Hochgrubachspitze", 47.5583658, 12.3433997, 2277, 94, 0, 0, 0 });
    m_labels.push_back({ "Östliche Hochgrubachspitze", 47.5587933, 12.3450985, 2284, 94, 0, 0, 0 });
    m_labels.push_back({ "Scheffauer", 47.5573214, 12.2418396, 2111, 97, 0, 0, 0 });
    m_labels.push_back({ "Maukspitze", 47.5588954, 12.3563668, 2231, 97, 0, 0, 0 });
    m_labels.push_back({ "Ellmauer Halt", 47.5616377, 12.3025296, 2342, 98, 0, 0, 0 });

    // Big mountains
    m_labels.push_back({ "Großglockner", 47.07455, 12.69388, 3798, 9, 0, 0});
    m_labels.push_back({ "Wildspitze", 46.88524, 10.86728, 3768, 9, 0, 0});
    m_labels.push_back({ "Großvenediger", 47.10927, 12.34534, 3657, 9, 0, 0});
    m_labels.push_back({ "Hochalmspitze", 47.01533, 13.32050, 3360, 9, 0, 0});
    m_labels.push_back({ "Piz Buin", 46.84412, 10.11889, 3312, 9, 0, 0});
    m_labels.push_back({ "Hoher Dachstein", 47.47519, 13.60569, 2995, 8, 0, 0});
    m_labels.push_back({ "Valluga", 47.15757, 10.21309, 2811, 8, 0, 0});
    m_labels.push_back({ "Birkkarspitze", 47.41129, 11.43765, 2749, 8, 0, 0});
    m_labels.push_back({ "Schönfeldspitze", 47.45831, 12.93774, 2653, 8, 0, 0});
    m_labels.push_back({ "Großer Priel", 47.71694, 14.06325, 2515, 8, 0, 0});
    m_labels.push_back({ "Ellmauer Halt", 47.5616377, 12.3025296, 2342, 8, 0, 0, 0 });
    m_labels.push_back({ "Hochschwab", 47.61824, 15.14245, 2277, 8, 0, 0});
    m_labels.push_back({ "Klosterwappen", 47.76706, 15.80450, 2076, 8, 0, 0});
    m_labels.push_back({ "Ötscher", 47.86186, 15.20251, 1893, 7, 0, 0});
    m_labels.push_back({ "Schafberg", 47.77639, 13.43389, 1783, 7, 0, 0});
    m_labels.push_back({ "Geschriebenstein", 47.35283, 16.43372, 884, 7, 0, 0});
    m_labels.push_back({ "Hermannskogel", 48.27072, 16.29456, 544, 7, 0, 0});

    // Schafbergbahn
    m_labels.push_back({ "Start", 47.73963, 13.43980, 542, 18, 0, 0});
//    m_labels.push_back({ "Schafbergbahn Mitte", 47.76980, 13.42216, 1363, 16, 0, 0});
    m_labels.push_back({ "Finish", 47.77512, 13.43400, 1732, 18, 0, 0});

    // Hochalpenstrasse north
    m_labels.push_back({ "Start", 47.14249, 12.81251, 1660, 28});
    m_labels.push_back({ "Finish", 47.11744, 12.82761, 2400, 28});

    // Hochalpenstrasse south
    m_labels.push_back({ "Start", 47.06347, 12.81843, 1870, 38});
//    m_labels.push_back({ "Hochalpenstraße B", 47.12143, 12.82044, 2100, 36});
    m_labels.push_back({ "Finish", 47.08099, 12.84253, 2504, 38});

    // Schafberg
//    m_labels.push_back({ "Schafberg", 47.77639, 13.43389, 1783, 28, 0, 0});
//    m_labels.push_back({ "St. Wolfgang", 47.73787, 13.44827, 548, 26, 0, 0});
//    m_labels.push_back({ "Hoher Dachstein", 47.47519, 13.60569, 2995, 28, 0, 0});
//    m_labels.push_back({ "Brennerin", 47.81659, 13.57866, 1605, 27, 0, 0});
//    m_labels.push_back({ "Drachenwand", 47.81377, 13.34725, 1176, 27, 0, 0});
//    m_labels.push_back({ "Unterach", 47.80431, 13.48897, 477, 27, 0, 0});
//    m_labels.push_back({ "Krottensee", 47.78318, 13.38849, 577, 27, 0, 0});

    // Dachstein
    m_labels.push_back({ "Dachstein", 47.47519, 13.60569, 3020, 206});
    m_labels.push_back({ "A-1", 47.42176, 13.65328, 1135, 208}); // visible - ramsau
    m_labels.push_back({ "A-2", 47.56217, 13.64867, 531, 208}); // hidden - hallstadt
    m_labels.push_back({ "A-3", 47.46563, 13.58354, 2189, 208}); // hidden - rauchegg
    m_labels.push_back({ "A-4", 47.77639, 13.43389, 1783, 208, 0, 0}); // visible - schafberg - far

//    m_labels.push_back({ "Dachstein", 47.47519, 13.60569, 3020, 206});
    m_labels.push_back({ "B-1", 47.48712, 13.65160, 2482, 208}); // hidden - moderstein
    m_labels.push_back({ "B-2", 47.63990, 13.76406, 732, 208}); // visible - altausee
//    m_labels.push_back({ "B-3", 47.45741, 13.55583, 2245, 208}); // visible - rettenstein
    m_labels.push_back({ "B-3", 47.48285, 13.56803, 2453, 208}); // hidden - hochkesselkopf
    m_labels.push_back({ "B-4", 47.26627, 13.76064, 2862, 208}); // visible - hochgolling - far

//    m_labels.push_back({ "Dachstein", 47.47519, 13.60569, 3020, 206});
    m_labels.push_back({ "C-1", 47.52787, 13.50790, 920 ,208}); // visible - gosausee
    m_labels.push_back({ "C-2", 47.55803, 13.68529, 520, 208}); // hidden - obertraun
    m_labels.push_back({ "C-3", 47.46262, 13.66600, 2539, 208}); // hidden - landfriedstein
    m_labels.push_back({ "C-4", 47.49373, 13.29833, 2247, 208}); // visible - tauernkogel - far

    // Grubenkarspitze
    //    m_labels.push_back({ "Sunntigerspitze", 47.36470, 11.48137, 2322, 6, 0, 0});
    //    m_labels.push_back({ "Reps", 47.36612, 11.46206, 2160, 6, 0, 0});
    m_labels.push_back({ "Grubenkarspitze", 47.38078, 11.52211, 2663, 48, 0, 0});
    m_labels.push_back({ "Ridge A", 47.36470, 11.48137, 2322, 46, 0, 0});
    m_labels.push_back({ "Grubenkarspitze", 47.38078, 11.52211, 2663, 58, 0, 0});
    m_labels.push_back({ "Ridge B", 47.38641, 11.47763, 2668, 56, 0, 0});
    m_labels.push_back({ "Grubenkarspitze", 47.38078, 11.52211, 2663, 68, 0, 0});
    m_labels.push_back({ "Ridge C", 47.37402, 11.57160, 2573, 66, 0, 0});

    // Ötschergräben
    m_labels.push_back({ "Start", 47.85239, 15.28817, 630, 78, 0, 0}); // Kraftwerk Wienerbruck
    m_labels.push_back({ "Finish", 47.84408, 15.24968, 705, 78, 0, 0});
//    m_labels.push_back({ "Finish", 47.84454, 15.24895, 770, 78, 0, 0}); // Mirafall
//    m_labels.push_back({ "Schleierfall", 47.84124, 15.21863, 800, 76, 0, 0});

    m_labels.push_back({ "Start", 47.85621, 15.28423, 610, 88, 0, 0});
    m_labels.push_back({ "Finish", 47.87514, 15.26870, 540, 88 });

    // Aschach
    m_labels.push_back({ "Start", 48.37488, 13.94198, 300, 98 });
    m_labels.push_back({ "Finish", 48.37147, 13.89558, 350, 98 });

    // Gimpel
    m_labels.push_back({ "Gimpel", 47.50127, 10.61249, 2176, 108, 0, 0});
    m_labels.push_back({ "Rote Flüh", 47.49962, 10.60855, 2111, 107, 0, 0});
    m_labels.push_back({ "Otto-Mayr-Hütte", 47.50934, 10.61872, 1530, 107, 0, 0});

    // Seekarlspitze
    m_labels.push_back({ "Seekarlspitze", 47.45723, 11.77804, 2261, 118, 0, 0});
    m_labels.push_back({ "Mauritzalm", 47.44377, 11.76422, 1836, 117, 0, 0});
    m_labels.push_back({ "Ampmoosalm", 47.46141, 11.78092, 1790, 117, 0, 0});

    // Furgler
    m_labels.push_back({ "Furgler", 47.04033, 10.51186, 3004, 128 });
    m_labels.push_back({ "Moosbahn Bergstation", 47.03258, 10.52875, 2429, 127 });
    m_labels.push_back({ "Furglersee", 47.04569, 10.52726, 2458, 127 });
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

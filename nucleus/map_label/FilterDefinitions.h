/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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
#include <QVector2D>

namespace nucleus::maplabel {

struct FilterDefinitions {
    Q_GADGET

    //    Q_PROPERTY(bool peaks_visible READ peaks_visible WRITE set_peaks_visible NOTIFY filter_changed)
    //    Q_PROPERTY(bool cities_visible READ cities_visible WRITE set_cities_visible NOTIFY filter_changed)
    //    Q_PROPERTY(bool cottages_visible READ cottages_visible WRITE set_cottages_visible NOTIFY filter_changed)
    //    Q_PROPERTY(bool webcams_visible READ webcams_visible WRITE set_webcams_visible NOTIFY filter_changed)
    //    Q_PROPERTY(QVector2D elevation_range READ elevation_range WRITE set_elevation_range NOTIFY filter_changed)

public:
    bool m_peaks_visible = true;
    bool m_cities_visible = true;
    bool m_cottages_visible = true;
    bool m_webcams_visible = true;

    bool m_peak_ele_range_filtered = false;
    QVector2D m_peak_ele_range = QVector2D(0, 4000);

    bool operator==(const FilterDefinitions&) const = default;
    bool operator!=(const FilterDefinitions&) const = default;

    Q_PROPERTY(bool peaks_visible MEMBER m_peaks_visible)
    Q_PROPERTY(bool cities_visible MEMBER m_cities_visible)
    Q_PROPERTY(bool cottages_visible MEMBER m_cottages_visible)
    Q_PROPERTY(bool webcams_visible MEMBER m_webcams_visible)
    Q_PROPERTY(QVector2D peak_ele_range MEMBER m_peak_ele_range)
};

} // namespace nucleus::maplabel

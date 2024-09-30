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
public:
    bool m_peaks_visible = true;
    bool m_cities_visible = true;
    bool m_cottages_visible = true;
    bool m_webcams_visible = true;

    QVector2D m_peak_ele_range = QVector2D(0, 4000);
    // false -> show both; true only show where cross is not null
    bool m_peak_has_cross = false;
    // false -> show both; true only show where register is not null
    bool m_peak_has_register = false;

    QVector2D m_city_population_range = QVector2D(0, 2000000);

    // false -> show both; true only show where shower is not null
    bool m_cottage_has_shower = false;
    // false -> show both; true show where phone and/or email is not null
    bool m_cottage_has_contact = false;
    QVector2D m_cottage_ele_range = QVector2D(0, 4000);

    bool operator==(const FilterDefinitions&) const = default;
    bool operator!=(const FilterDefinitions&) const = default;

    Q_PROPERTY(bool peaks_visible MEMBER m_peaks_visible)
    Q_PROPERTY(bool cities_visible MEMBER m_cities_visible)
    Q_PROPERTY(bool cottages_visible MEMBER m_cottages_visible)
    Q_PROPERTY(bool webcams_visible MEMBER m_webcams_visible)
    Q_PROPERTY(QVector2D peak_ele_range MEMBER m_peak_ele_range)
    Q_PROPERTY(bool peak_has_cross MEMBER m_peak_has_cross)
    Q_PROPERTY(bool peak_has_register MEMBER m_peak_has_register)
    Q_PROPERTY(QVector2D city_population_range MEMBER m_city_population_range)
    Q_PROPERTY(bool cottage_has_shower MEMBER m_cottage_has_shower)
    Q_PROPERTY(bool cottage_has_contact MEMBER m_cottage_has_contact)
    Q_PROPERTY(QVector2D cottage_ele_range MEMBER m_cottage_ele_range)
};

} // namespace nucleus::maplabel

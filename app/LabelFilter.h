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

struct FilterDefinitions {
public:
    bool m_peaks_visible = true;
    bool m_cities_visible = true;
    bool m_cottages_visible = true;

    bool m_peak_ele_range_filtered = false;
    QVector2D m_peak_ele_range = QVector2D(0,4000);
};

class LabelFilter : public QObject {
    Q_OBJECT
public:
    LabelFilter();
    ~LabelFilter();

    Q_PROPERTY(bool peaks_visible READ peaks_visible WRITE set_peaks_visible NOTIFY filter_changed)
    Q_PROPERTY(bool cities_visible READ cities_visible WRITE set_cities_visible NOTIFY filter_changed)
    Q_PROPERTY(bool cottages_visible READ cottages_visible WRITE set_cottages_visible NOTIFY filter_changed)
    Q_PROPERTY(QVector2D elevation_range READ elevation_range WRITE set_elevation_range NOTIFY filter_changed)

    QVector2D elevation_range() const;
    void set_elevation_range(const QVector2D &elevation_range);

    [[nodiscard]] bool peaks_visible() const;
    void set_peaks_visible(const bool &peaks_visible);
    [[nodiscard]] bool cities_visible() const;
    void set_cities_visible(const bool &cities_visible);
    [[nodiscard]] bool cottages_visible() const;
    void set_cottages_visible(const bool &cottages_visible);

public slots:
    void trigger_filter_update();

signals:
    //    void update_filter(FilterDefinitions);
    void filter_updated(FilterDefinitions);
    void filter_changed();

private:
    bool m_peaks_visible = true;
    bool m_cities_visible = true;
    bool m_cottages_visible = true;

    QVector2D m_elevation_range;

    FilterDefinitions m_filter_definitions;
    const FilterDefinitions m_default_filter_definitions;
};

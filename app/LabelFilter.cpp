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

#include "LabelFilter.h"

#include <iostream>

LabelFilter::LabelFilter()
{
    connect(this, &LabelFilter::filter_changed, this, &LabelFilter::trigger_filter_update);

    m_update_filter_timer = std::make_unique<QTimer>(this);
    m_update_filter_timer->setSingleShot(true);
    connect(m_update_filter_timer.get(), &QTimer::timeout, this, &LabelFilter::filter_update);

    // trigger the initial filter update
    m_update_filter_timer->start(m_update_filter_time);
}

LabelFilter::~LabelFilter() { qDebug("LabelFilter::~LabelFilter"); }

void LabelFilter::trigger_filter_update()
{
    if (!m_update_filter_timer->isActive())
        m_update_filter_timer->start(m_update_filter_time);
}

void LabelFilter::filter_update()
{
    // check against default values what changed
    m_filter_definitions.m_peak_ele_range_filtered = m_filter_definitions.m_peak_ele_range != m_default_filter_definitions.m_peak_ele_range;

    emit filter_updated(m_filter_definitions);
}

QVector2D LabelFilter::elevation_range() const { return m_filter_definitions.m_peak_ele_range; }

void LabelFilter::set_elevation_range(const QVector2D &elevation_range)
{
    m_filter_definitions.m_peak_ele_range = elevation_range;
    emit filter_changed();
}

bool LabelFilter::peaks_visible() const { return m_filter_definitions.m_peaks_visible; }
void LabelFilter::set_peaks_visible(const bool &peaks_visible)
{
    if (m_filter_definitions.m_peaks_visible == peaks_visible)
        return;
    m_filter_definitions.m_peaks_visible = peaks_visible;
    emit filter_changed();
}

bool LabelFilter::cities_visible() const { return m_filter_definitions.m_cities_visible; }
void LabelFilter::set_cities_visible(const bool &cities_visible)
{
    if (m_filter_definitions.m_cities_visible == cities_visible)
        return;
    m_filter_definitions.m_cities_visible = cities_visible;
    emit filter_changed();
}

bool LabelFilter::cottages_visible() const { return m_filter_definitions.m_cottages_visible; }
void LabelFilter::set_cottages_visible(const bool &cottages_visible)
{
    if (m_filter_definitions.m_cottages_visible == cottages_visible)
        return;
    m_filter_definitions.m_cottages_visible = cottages_visible;
    emit filter_changed();
}

bool LabelFilter::webcams_visible() const { return m_filter_definitions.m_webcams_visible; }
void LabelFilter::set_webcams_visible(const bool& webcams_visible)
{
    if (m_filter_definitions.m_webcams_visible == webcams_visible)
        return;
    m_filter_definitions.m_webcams_visible = webcams_visible;
    emit filter_changed();
}

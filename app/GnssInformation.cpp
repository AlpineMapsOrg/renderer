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

#include "GnssInformation.h"

#include <QGeoPositionInfoSource>

GnssInformation::GnssInformation()
    : m_position_source(QGeoPositionInfoSource::createDefaultSource(this))
{
    qDebug("GnssInformation::GnssInformation");
    if (!m_position_source) {
        qDebug("GnssInformation: No QGeoPositionInfoSource available!");
        return;
    }
    connect(m_position_source, &QGeoPositionInfoSource::positionUpdated, this, &GnssInformation::position_updated);
    connect(m_position_source, &QGeoPositionInfoSource::errorOccurred, this, [](QGeoPositionInfoSource::Error error) {
        QMetaEnum metaEnum = QMetaEnum::fromType<QGeoPositionInfoSource::Error>();
        qDebug() << "GnssInformation: QGeoPositionInfoSource::errorOccurred: " << metaEnum.valueToKey(error);
    });
}

GnssInformation::~GnssInformation()
{
    qDebug("GnssInformation::~GnssInformation");
}

double GnssInformation::latitude() const
{
    return m_latitude;
}

double GnssInformation::longitude() const
{
    return m_longitude;
}

double GnssInformation::altitude() const
{
    return m_altitude;
}

double GnssInformation::horizontal_accuracy() const
{
    return m_horizontal_accuracy;
}

QDateTime GnssInformation::timestamp() const
{
    return m_timestamp;
}

void GnssInformation::position_updated(const QGeoPositionInfo& position)
{
    m_latitude = position.coordinate().latitude();
    m_longitude = position.coordinate().longitude();
    m_altitude = position.coordinate().altitude();
    m_horizontal_accuracy = position.attribute(QGeoPositionInfo::Attribute::HorizontalAccuracy);
    m_timestamp = position.timestamp();
    emit information_updated();
}

bool GnssInformation::enabled() const
{
    return m_enabled;
}

void GnssInformation::set_enabled(bool new_enabled)
{
    if (m_enabled == new_enabled)
        return;
    m_enabled = new_enabled;
    if (m_enabled)
        m_position_source->startUpdates();
    else
        m_position_source->stopUpdates();

    emit enabled_changed();
}

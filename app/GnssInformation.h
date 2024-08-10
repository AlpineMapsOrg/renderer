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

#pragma once

#include <QDateTime>
#include <QQmlEngine>
#include <QQuickItem>

class QGeoPositionInfoSource;
class QGeoPositionInfo;

class GnssInformation : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
public:
    GnssInformation();
    ~GnssInformation();

    [[nodiscard]] double latitude() const;
    [[nodiscard]] double longitude() const;
    [[nodiscard]] double altitude() const;
    [[nodiscard]] double horizontal_accuracy() const;

    [[nodiscard]] QDateTime timestamp() const;

    bool enabled() const;
    void set_enabled(bool new_enabled);

signals:
    void information_updated();

    void enabled_changed();

private:
    void position_updated(const QGeoPositionInfo& position);

private:
    double m_latitude = 0;
    double m_longitude = 0;
    double m_altitude = 0;
    double m_horizontal_accuracy = 0;
    QDateTime m_timestamp = {};
    Q_PROPERTY(float latitude READ latitude NOTIFY information_updated)
    Q_PROPERTY(float longitude READ longitude NOTIFY information_updated)
    Q_PROPERTY(float altitude READ altitude NOTIFY information_updated)
    Q_PROPERTY(float horizontal_accuracy READ horizontal_accuracy NOTIFY information_updated)
    Q_PROPERTY(QDateTime timestamp READ timestamp NOTIFY information_updated)
    bool m_enabled = false;

    QGeoPositionInfoSource* m_position_source = nullptr;
    Q_PROPERTY(bool enabled READ enabled WRITE set_enabled NOTIFY enabled_changed)
};

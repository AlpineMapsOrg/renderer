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

class LabelFilter : public QObject {
    Q_OBJECT
public:
    LabelFilter();
    ~LabelFilter();

    [[nodiscard]] QVector2D elevationRange() const;

signals:
    void information_updated();

private slots:
    void filter_updated();

private:
    QVector2D m_elevation_range;

    Q_PROPERTY(QVector2D elevationRange READ elevationRange NOTIFY information_updated)


//    QGeoPositionInfoSource* m_position_source = nullptr;
};

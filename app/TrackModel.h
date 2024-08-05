/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
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

#include "nucleus/track/Manager.h"
#include <QObject>

class TrackModel : public QObject {
    Q_OBJECT
public:
    explicit TrackModel(QObject* parent = nullptr);

    Q_INVOKABLE QPointF lat_long(unsigned index);
    Q_INVOKABLE unsigned n_tracks() const { return m_data.size(); }

    float display_width() const;
    void set_display_width(float new_display_width);

    unsigned int shading_style() const;
    void set_shading_style(unsigned int new_shading_style);

public slots:
    void upload_track();

signals:
    void tracks_changed(const QVector<nucleus::track::Gpx>& tracks);

    void display_width_changed(float display_width);

    void shading_style_changed(unsigned int shading_style);

private:
    QVector<nucleus::track::Gpx> m_data;

    float m_display_width = 7.f;
    unsigned m_shading_style = 0u;
    Q_PROPERTY(float display_width READ display_width WRITE set_display_width NOTIFY display_width_changed FINAL)
    Q_PROPERTY(unsigned int shading_style READ shading_style WRITE set_shading_style NOTIFY shading_style_changed FINAL)
};

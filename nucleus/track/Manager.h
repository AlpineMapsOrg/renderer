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

#include "nucleus/track/GPX.h"
#include <QObject>

namespace nucleus::track {

class Manager : public QObject {
    Q_OBJECT
public:
    Manager(QObject* parent);

public slots:
    virtual void change_tracks(const QVector<nucleus::track::Gpx>& tracks) = 0;
    virtual void change_display_width(float new_width) = 0;
    virtual void change_shading_style(unsigned new_style) = 0;
};
} // namespace nucleus::track

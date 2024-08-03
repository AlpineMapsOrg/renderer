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

#include "nucleus/utils/GPX.h"
#include <QObject>

namespace nucleus::track {

using Id = uint64_t;

class Manager : public QObject {
    Q_OBJECT
public:
    explicit Manager(QObject* parent = nullptr);

    std::vector<nucleus::gpx::Gpx> tracks() const;
    nucleus::gpx::Gpx track(Id id) const;

public slots:
    void add_or_replace(Id id, const nucleus::gpx::Gpx& gpx);
    void remove(Id id);
    unsigned size() const;
signals:
    void tracks_changed(const QVector<nucleus::gpx::Gpx>& tracks);

private:
    std::unordered_map<Id, nucleus::gpx::Gpx> m_data;
};
} // namespace nucleus::track

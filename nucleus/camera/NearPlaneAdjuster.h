/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <QObject>

#include "sherpa/tile.h"

namespace nucleus {
struct Tile;
}

namespace nucleus::camera {
class Definition;

class NearPlaneAdjuster : public QObject
{
    Q_OBJECT
public:
    explicit NearPlaneAdjuster(QObject *parent = nullptr);

signals:
    void near_plane_changed(float new_distance) const;

public slots:
    void update_camera(const Definition& new_definition);
    void add_tile(const std::shared_ptr<nucleus::Tile>& tile);
    void remove_tile(const tile::Id& tile_id);

private:
    void update_near_plane() const;

    struct Object {
        Object() = default;
        Object(const tile::Id& id, const double& elevation) : id(id), elevation(elevation) {}
        tile::Id id;
        double elevation;
    };
    glm::dvec3 m_camera_position;
    std::vector<Object> m_objects;
};

}

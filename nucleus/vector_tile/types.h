/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Lucas Dworschak
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

#include <QMap>
#include <QObject>
#include <QString>
#include <cstdint>
#include <glm/glm.hpp>
#include <radix/tile.h>

namespace nucleus::vector_tile {

struct PointOfInterest {
    Q_GADGET
public:
    enum class Type { Unknown, Peak, Settlement, AlpineHut, Webcam };
    Q_ENUM(Type)
    uint64_t id = unsigned(-1);
    Type type = Type::Unknown;
    QString name;
    glm::dvec3 lat_long_alt = glm::dvec3(0);
    glm::dvec3 world_space_pos = glm::dvec3(0);
    float importance = 0;
    QMap<QString, QString> attributes;
    // write parser
    // use for rendering
    // use for picking
};
using PointOfInterestCollection = std::shared_ptr<const std::vector<PointOfInterest>>;
using PointOfInterestTileCollection = std::unordered_map<tile::Id, PointOfInterestCollection, tile::Id::Hasher>;

} // namespace nucleus::vector_tile

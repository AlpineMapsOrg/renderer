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

#include <glm/glm.hpp>
#include <mapbox/vector_tile.hpp>

#include <QString>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace nucleus {
class DataQuerier;
}

namespace nucleus::vectortile {

// note ENUM_END is used here, so that we can iterate over all featuretypes (by using ENUM END as end condition)
enum FeatureType { Peak = 0, City = 1, ENUM_END = 2 };

struct FeatureTXT {
    virtual ~FeatureTXT() = default;
    unsigned long id;
    QString name;
    glm::dvec2 position; // latitude / Longitude
    glm::dvec3 worldposition;
    FeatureType type;

    // determine the highest zoom level where the height has been calculated
    int highest_zoom = 0;

    static void parse(std::shared_ptr<FeatureTXT> ft, const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    // void updateWorldPosition(int zoom_level, std::shared_ptr<DataQuerier> dataquerier)
    // {
    //     if (zoom_level > highest_zoom)
    //         worldposition.z = dataquerier->get_altitude(position);
    // }

    virtual QString labelText() = 0;
};

struct FeatureTXTPeak : public FeatureTXT {
    int elevation;

    static std::shared_ptr<FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    QString labelText();
};

typedef std::unordered_map<FeatureType, std::unordered_set<std::shared_ptr<FeatureTXT>>> VectorTile;

} // namespace nucleus::vectortile

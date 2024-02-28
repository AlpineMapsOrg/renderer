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

#include "radix/tile.h"
#include <glm/glm.hpp>
#include <mapbox/vector_tile.hpp>

#include <QString>
#include <iostream>
#include <memory>
#include <string>

#include "nucleus/DataQuerier.h"

namespace nucleus {

// note ENUM_END is used here, so that we can iterate over all featuretypes
enum FeatureType { Peak = 0, City = 1, ENUM_END = 2 };

struct FeatureTXT {
    unsigned long id;
    QString name;
    glm::dvec3 position;
    FeatureType type;

    // determine the highest zoom level where the height has been calculated
    int highest_zoom = 0;

    void print() const { std::cout << name.constData() << "\t\t" << glm::to_string(position) << "\t" << id << std::endl; }

    static void parse(std::shared_ptr<FeatureTXT> ft, const mapbox::vector_tile::feature& feature, const tile::SrsBounds& tile_bounds, const double extent)
    {
        ft->id = feature.getID().get<uint64_t>();

        // height position(z) is initialized with -1000 since we are loading it from the height data
        // we could also set it from the elevation attribute, but this attribute does not necessarily exist (e.g. city names)
        // -> it is therefore more future proof to load it separately from our own height data (once it is available)
        mapbox::vector_tile::points_arrays_type geom = feature.getGeometries<mapbox::vector_tile::points_arrays_type>(1.0);
        glm::dvec2 pos;
        for (auto const& point_array : geom) {
            for (auto const& point : point_array) {
                pos = tile_bounds.min + glm::dvec2(point.x / extent, point.y / extent) * (tile_bounds.max - tile_bounds.min);
            }
        }
        ft->position = glm::dvec3(pos.x, pos.y, -1000.0);

        auto props = feature.getProperties();
        ft->name = QString::fromStdString(props["name"].get<std::string>());
    }

    void calculateHeight(int zoomLevel, DataQuerier* dataquerier)
    {
        if (!dataquerier)
            return;

        if (zoomLevel > highest_zoom) {
            // TODO we are currently transforming world coordinates to lat/long, so that they can be converted again into world coords by get_altitude
            // position.z = dataquerier->get_altitude(srs::world_to_lat_long(glm::dvec2(position.x, position.y)));
            // position.z = dataquerier->get_altitude(srs::world_to_lat_long(glm::dvec2(position.x, position.y)));
            highest_zoom = zoomLevel;
        }
    }

    virtual QString labelText() = 0;
};

struct FeatureTXTPeak : public FeatureTXT {
    int elevation;

    static std::shared_ptr<FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const tile::SrsBounds& tile_bounds, const double extent)
    {
        auto peak = std::make_shared<FeatureTXTPeak>();
        FeatureTXT::parse(peak, feature, tile_bounds, extent);

        peak->type = FeatureType::Peak;

        auto props = feature.getProperties();
        peak->elevation = (int)props["elevation"].get<std::int64_t>();
        // TODO remove this once altitude calcualation works...
        glm::vec2 latlong = srs::world_to_lat_long(glm::dvec2(peak->position.x, peak->position.y));
        peak->position = srs::lat_long_alt_to_world({ latlong.x, latlong.y, peak->elevation });
        return peak;
    }

    QString labelText() { return QString("%1 (%2m)").arg(name).arg(double(elevation), 0, 'f', 0); }
};

} // namespace nucleus

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
#include <radix/tile.h>

#include <QString>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace nucleus {
class DataQuerier;
}

namespace nucleus::vectortile {

// note ENUM_END is used here, so that we can iterate over all featuretypes (by using ENUM END as end condition)
enum FeatureType { Peak = 0, City = 1, Cottage = 2, ENUM_END = 3 };

struct FeatureTXT {
    unsigned long id;
    QString name;
    // importance: value [0,1] whereas 1 is highest importance
    // importance is a normalized value that encapsulates the distance to the next POI with a higher importance value than itself
    // if the value is 1 there are no more important POIs within the search area
    float importance;
    // importance_metric: metric with which importance is calculated (if two values have same importance, a higher value here should triumph
    // example values: for peaks=elevation, for cities=population
    int importance_metric;

    glm::dvec2 position; // latitude / Longitude
    glm::dvec3 worldposition;
    FeatureType type;


    // determine the highest zoom level where the height has been calculated
    int highest_zoom = 0;

    static void parse(std::shared_ptr<FeatureTXT> ft, const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    virtual QString labelText() const = 0;
};

struct FeatureTXTPeak : public FeatureTXT {
    QString de_name;
    QString wikipedia;
    QString wikidata;
    QString importance_osm;
    QString prominence;
    QString summit_cross;
    QString summit_register;
    int elevation;

    static std::shared_ptr<const FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    QString labelText() const;
};

struct FeatureTXTCity : public FeatureTXT {
    QString de_name;
    QString wikipedia;
    QString wikidata;
    int population;
    QString place;
    QString population_date;
    QString website;

    static std::shared_ptr<const FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    QString labelText() const;
};

struct FeatureTXTCottage : public FeatureTXT {
    QString de_name;
    QString wikipedia;
    QString wikidata;
    QString description;
    QString capacity;
    QString opening_hours;
    QString reservation;
    QString electricity;
    QString shower;
    QString winter_room;
    QString phone;
    QString website;
    QString seasonal;
    QString internet_access;
    QString fee;
    QString addr_city;
    QString addr_street;
    QString addr_postcode;
    QString addr_housenumber;
    QString operators;
    QString feature_type;
    QString access;
    int elevation;

    static std::shared_ptr<const FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    QString labelText() const;
};

typedef std::unordered_set<std::shared_ptr<const FeatureTXT>> VectorTile;
typedef std::unordered_map<tile::Id, nucleus::vectortile::VectorTile, tile::Id::Hasher> TiledVectorTile;

} // namespace nucleus::vectortile

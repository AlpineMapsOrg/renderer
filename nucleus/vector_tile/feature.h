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

#include <glm/glm.hpp>
#include <mapbox/vector_tile.hpp>
#include <radix/tile.h>

#include <QString>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "nucleus/picker/PickerTypes.h"

namespace nucleus {
class DataQuerier;
}

using namespace nucleus::picker;

namespace nucleus::vector_tile {

// note ENUM_END is used here, so that we can iterate over all featuretypes (by using ENUM END as end condition)
enum FeatureType { Peak = 0, City = 1, Cottage = 2, Webcam = 3, ENUM_END = 4 };

struct FeatureTXT {
    FeatureTXT();
    virtual ~FeatureTXT() = default;

    const uint64_t internal_id = 0;
    uint64_t id;
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

    inline static uint64_t id_counter = 1UL;

    static void parse(std::shared_ptr<FeatureTXT> ft, const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    virtual QString label_text() const = 0;

    virtual FeatureProperties get_feature_data() const = 0;
};

struct FeatureTXTPeak : public FeatureTXT {
    QString wikipedia;
    QString wikidata;
    QString importance_osm;
    QString prominence;
    QString summit_cross;
    QString summit_register;
    int elevation = 0;

    static std::shared_ptr<const FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    QString label_text() const;
    FeatureProperties get_feature_data() const;
};

struct FeatureTXTCity : public FeatureTXT {
    QString wikipedia;
    QString wikidata;
    int population;
    QString place; // city,town,village,hamlet
    QString population_date;
    QString website;

    static std::shared_ptr<const FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    QString label_text() const;
    FeatureProperties get_feature_data() const;
};

struct FeatureTXTCottage : public FeatureTXT {
    QString wikipedia;
    QString wikidata;
    QString description;
    QString capacity;
    QString opening_hours;
    QString shower;
    QString phone;
    QString email;
    QString website;
    QString internet_access;
    QString addr_city;
    QString addr_street;
    QString addr_postcode;
    QString addr_housenumber;
    QString operators;
    QString feature_type;
    QString access;
    int elevation;

    static std::shared_ptr<const FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    QString label_text() const;
    FeatureProperties get_feature_data() const;
};

struct FeatureTXTWebcam : public FeatureTXT {
    // camera_type: dome,fixed,panning,panorama,NULL
    QString camera_type;
    // direction: 0-350 // -1 if unknown
    int direction;
    // surveillance_type: camera,fixed,guard,outdoor,webcam,NULL
    QString surveillance_type;
    // surveillance_zone: area,atm,building,parking,public,station,street,town,traffic,NULL
    // not used since data seems irrelevant + is also often wrong e.g. foto-webcam.eu only has atm and town zone
    // although it doesnt show any atms and mostly area views
    //    QString surveillance_zone;
    // link to live image site
    QString image;
    // some description about the scene of the webcam
    QString description;
    // elevation in meters
    int elevation;

    static std::shared_ptr<const FeatureTXT> parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier);

    QString label_text() const;
    FeatureProperties get_feature_data() const;
};

using FeatureSet = std::unordered_set<std::shared_ptr<const FeatureTXT>>;
using FeatureSetTiles = std::unordered_map<tile::Id, nucleus::vector_tile::FeatureSet, tile::Id::Hasher>;

std::shared_ptr<FeatureSet> parse(const QByteArray& vectorTileData, const std::shared_ptr<DataQuerier> dataquerier);

} // namespace nucleus::vector_tile

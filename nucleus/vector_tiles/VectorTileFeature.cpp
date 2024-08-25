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

#include "VectorTileFeature.h"

#include "nucleus/DataQuerier.h"

#include <regex>

namespace nucleus::vectortile {

// NOTE if you get the following error (since it just happened again for me):
    // terminate called after throwing an instance of 'mapbox::util::bad_variant_access'
    // what():  in get<T>()
// this means that the variable that mapbox wants to parse is not of the actual type T
// one possible error here might be that the actual type is empty/null due to a caching issue
// -> try to clear the tile_cache
// alternatively you can also use .get_unchecked<T>() and maybe debug why this is happening

void FeatureTXT::parse(std::shared_ptr<FeatureTXT> ft, const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    ft->id = get<uint64_t>(feature.getID());

    auto props = feature.getProperties();
    ft->name = QString::fromStdString(get<std::string>(props["name"]));
    ft->position = glm::dvec2(get<double>(props["lat"]), get<double>(props["long"]));

    if (holds_alternative<double>(props["importance"]))
        ft->importance = get<double>(props["importance"]);
    if (holds_alternative<std::uint64_t>(props["importance_metric"]))
        ft->importance_metric = get<std::uint64_t>(props["importance_metric"]);

    double altitude = 0;
    if (dataquerier)
        altitude = dataquerier->get_altitude(ft->position);

    ft->worldposition = nucleus::srs::lat_long_alt_to_world(glm::dvec3(ft->position.x, ft->position.y, altitude));
}

std::shared_ptr<const FeatureTXT> FeatureTXTPeak::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto peak = std::make_shared<FeatureTXTPeak>();
    FeatureTXT::parse(peak, feature, dataquerier);

    peak->type = FeatureType::Peak;

    auto props = feature.getProperties();

    if (holds_alternative<std::string>(props["de_name"]))
        peak->de_name = QString::fromStdString(get<std::string>(props["de_name"]));
    if (holds_alternative<std::string>(props["wikipedia"]))
        peak->wikipedia = QString::fromStdString(get<std::string>(props["wikipedia"]));
    if (holds_alternative<std::string>(props["wikidata"]))
        peak->wikidata = QString::fromStdString(get<std::string>(props["wikidata"]));
    if (holds_alternative<std::string>(props["importance_osm"]))
        peak->importance_osm = QString::fromStdString(get<std::string>(props["importance_osm"]));
    if (holds_alternative<std::string>(props["prominence"]))
        peak->prominence = QString::fromStdString(get<std::string>(props["prominence"]));
    if (holds_alternative<std::string>(props["summit_cross"]))
        peak->summit_cross = QString::fromStdString(get<std::string>(props["summit_cross"]));
    if (holds_alternative<std::string>(props["summit_register"]))
        peak->summit_register = QString::fromStdString(get<std::string>(props["summit_register"]));
    if (holds_alternative<uint64_t>(props["ele"]))
        peak->elevation = get<std::uint64_t>(props["ele"]);

    return peak;
}

QString FeatureTXTPeak::labelText() const
{
    if (elevation == 0)
        return name;
    else
        return QString("%1 (%2m)").arg(name).arg(double(elevation), 0, 'f', 0);
}

std::shared_ptr<const FeatureTXT> FeatureTXTCity::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto city = std::make_shared<FeatureTXTCity>();
    FeatureTXT::parse(city, feature, dataquerier);

    city->type = FeatureType::City;

    auto props = feature.getProperties();

    if (holds_alternative<std::string>(props["de_name"]))
        city->de_name = QString::fromStdString(get<std::string>(props["de_name"]));
    if (holds_alternative<std::string>(props["wikipedia"]))
        city->wikipedia = QString::fromStdString(get<std::string>(props["wikipedia"]));
    if (holds_alternative<std::string>(props["wikidata"]))
        city->wikidata = QString::fromStdString(get<std::string>(props["wikidata"]));
    if (holds_alternative<std::uint64_t>(props["population"]))
        city->population = get<std::uint64_t>(props["population"]);
    if (holds_alternative<std::string>(props["place"]))
        city->place = QString::fromStdString(get<std::string>(props["place"]));
    if (holds_alternative<std::string>(props["population_date"]))
        city->population_date = QString::fromStdString(get<std::string>(props["population_date"]));
    if (holds_alternative<std::string>(props["website"]))
        city->website = QString::fromStdString(get<std::string>(props["website"]));

    return city;
}

QString FeatureTXTCity::labelText() const { return name; }

std::shared_ptr<const FeatureTXT> FeatureTXTCottage::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto cottage = std::make_shared<FeatureTXTCottage>();
    FeatureTXT::parse(cottage, feature, dataquerier);

    cottage->type = FeatureType::Cottage;

    auto props = feature.getProperties();

    if (holds_alternative<std::string>(props["de_name"]))
        cottage->de_name = QString::fromStdString(get<std::string>(props["de_name"]));
    if (holds_alternative<std::string>(props["wikipedia"]))
        cottage->wikipedia = QString::fromStdString(get<std::string>(props["wikipedia"]));
    if (holds_alternative<std::string>(props["wikidata"]))
        cottage->wikidata = QString::fromStdString(get<std::string>(props["wikidata"]));
    if (holds_alternative<std::string>(props["description"]))
        cottage->description = QString::fromStdString(get<std::string>(props["description"]));
    if (holds_alternative<std::string>(props["capacity"]))
        cottage->capacity = QString::fromStdString(get<std::string>(props["capacity"]));
    if (holds_alternative<std::string>(props["opening_hours"]))
        cottage->opening_hours = QString::fromStdString(get<std::string>(props["opening_hours"]));
    if (holds_alternative<std::string>(props["reservation"]))
        cottage->reservation = QString::fromStdString(get<std::string>(props["reservation"]));
    if (holds_alternative<std::string>(props["electricity"]))
        cottage->electricity = QString::fromStdString(get<std::string>(props["electricity"]));
    if (holds_alternative<std::string>(props["shower"]))
        cottage->shower = QString::fromStdString(get<std::string>(props["shower"]));
    if (holds_alternative<std::string>(props["winter_room"]))
        cottage->winter_room = QString::fromStdString(get<std::string>(props["winter_room"]));
    if (holds_alternative<std::string>(props["phone"]))
        cottage->phone = QString::fromStdString(get<std::string>(props["phone"]));
    if (holds_alternative<std::string>(props["website"]))
        cottage->website = QString::fromStdString(get<std::string>(props["website"]));
    if (holds_alternative<std::string>(props["seasonal"]))
        cottage->seasonal = QString::fromStdString(get<std::string>(props["seasonal"]));
    if (holds_alternative<std::string>(props["internet_access"]))
        cottage->internet_access = QString::fromStdString(get<std::string>(props["internet_access"]));
    if (holds_alternative<std::string>(props["fee"]))
        cottage->fee = QString::fromStdString(get<std::string>(props["fee"]));
    if (holds_alternative<std::string>(props["addr_city"]))
        cottage->addr_city = QString::fromStdString(get<std::string>(props["addr_city"]));
    if (holds_alternative<std::string>(props["addr_street"]))
        cottage->addr_street = QString::fromStdString(get<std::string>(props["addr_street"]));
    if (holds_alternative<std::string>(props["addr_postcode"]))
        cottage->addr_postcode = QString::fromStdString(get<std::string>(props["addr_postcode"]));
    if (holds_alternative<std::string>(props["addr_housenumber"]))
        cottage->addr_housenumber = QString::fromStdString(get<std::string>(props["addr_housenumber"]));
    if (holds_alternative<std::string>(props["operator"]))
        cottage->operators = QString::fromStdString(get<std::string>(props["operator"]));
    if (holds_alternative<std::string>(props["feature_type"]))
        cottage->feature_type = QString::fromStdString(get<std::string>(props["feature_type"]));
    if (holds_alternative<std::string>(props["access"]))
        cottage->access = QString::fromStdString(get<std::string>(props["access"]));
    if (holds_alternative<uint64_t>(props["ele"]))
        cottage->elevation = get<std::uint64_t>(props["ele"]);

    return cottage;
}

QString FeatureTXTCottage::labelText() const { return name; }

std::shared_ptr<const FeatureTXT> FeatureTXTWebcam::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto webcam = std::make_shared<FeatureTXTWebcam>();
    FeatureTXT::parse(webcam, feature, dataquerier);

    webcam->type = FeatureType::Webcam;

    auto props = feature.getProperties();

    if (holds_alternative<std::string>(props["camera_type"]))
        webcam->camera_type = QString::fromStdString(get<std::string>(props["camera_type"]));
    if (holds_alternative<uint64_t>(props["direction"]))
        webcam->direction = get<std::uint64_t>(props["direction"]);
    if (holds_alternative<std::string>(props["surveillance_type"]))
        webcam->surveillance_type = QString::fromStdString(get<std::string>(props["surveillance_type"]));
    if (holds_alternative<std::string>(props["image"]))
        webcam->image = QString::fromStdString(get<std::string>(props["image"]));
    if (holds_alternative<std::string>(props["description"]))
        webcam->description = QString::fromStdString(get<std::string>(props["description"]));
    if (holds_alternative<uint64_t>(props["ele"]))
        webcam->elevation = get<std::uint64_t>(props["ele"]);

    return webcam;
}

QString FeatureTXTWebcam::labelText() const { return ""; /* no text*/ }

} // namespace nucleus::vectortile

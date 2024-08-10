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
    ft->id = feature.getID().get<uint64_t>();

    auto props = feature.getProperties();
    ft->name = QString::fromStdString(props["name"].get<std::string>());
    ft->position = glm::dvec2(props["lat"].get<double>(), props["long"].get<double>());

    if (props["importance"].valid() && props["importance"].is<double>())
        ft->importance = props["importance"].get<double>();
    if (props["importance_metric"].valid() && props["importance_metric"].is<std::uint64_t>())
        ft->importance_metric = props["importance_metric"].get<std::uint64_t>();

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

    if (props["de_name"].valid() && props["de_name"].is<std::string>())
        peak->de_name = QString::fromStdString(props["de_name"].get<std::string>());
    if (props["wikipedia"].valid() && props["wikipedia"].is<std::string>())
        peak->wikipedia = QString::fromStdString(props["wikipedia"].get<std::string>());
    if (props["wikidata"].valid() && props["wikidata"].is<std::string>())
        peak->wikidata = QString::fromStdString(props["wikidata"].get<std::string>());
    if (props["importance_osm"].valid() && props["importance_osm"].is<std::string>())
        peak->importance_osm =  QString::fromStdString(props["importance_osm"].get<std::string>());
    if (props["prominence"].valid() && props["prominence"].is<std::string>())
        peak->prominence =  QString::fromStdString(props["prominence"].get<std::string>());
    if (props["summit_cross"].valid() && props["summit_cross"].is<std::string>())
        peak->summit_cross = QString::fromStdString(props["summit_cross"].get<std::string>());
    if (props["summit_register"].valid() && props["summit_register"].is<std::string>())
        peak->summit_register = QString::fromStdString(props["summit_register"].get<std::string>());
    if (props["ele"].valid() && props["ele"].is<uint64_t>())
        peak->elevation = props["ele"].get<std::uint64_t>();

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

    if (props["de_name"].valid() && props["de_name"].is<std::string>())
        city->de_name = QString::fromStdString(props["de_name"].get<std::string>());
    if (props["wikipedia"].valid() && props["wikipedia"].is<std::string>())
        city->wikipedia = QString::fromStdString(props["wikipedia"].get<std::string>());
    if (props["wikidata"].valid() && props["wikidata"].is<std::string>())
        city->wikidata = QString::fromStdString(props["wikidata"].get<std::string>());
    if (props["population"].valid() && props["population"].is<std::uint64_t>())
        city->population = props["population"].get<std::uint64_t>();
    if (props["place"].valid() && props["place"].is<std::string>())
        city->place = QString::fromStdString(props["place"].get<std::string>());
    if (props["population_date"].valid() && props["population_date"].is<std::string>())
        city->population_date = QString::fromStdString(props["population_date"].get<std::string>());
    if (props["website"].valid() && props["website"].is<std::string>())
        city->website = QString::fromStdString(props["website"].get<std::string>());


    return city;
}

QString FeatureTXTCity::labelText() const { return name; }

std::shared_ptr<const FeatureTXT> FeatureTXTCottage::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto cottage = std::make_shared<FeatureTXTCottage>();
    FeatureTXT::parse(cottage, feature, dataquerier);

    cottage->type = FeatureType::Cottage;

    auto props = feature.getProperties();

    if (props["de_name"].valid() && props["de_name"].is<std::string>())
        cottage->de_name = QString::fromStdString(props["de_name"].get<std::string>());
    if (props["wikipedia"].valid() && props["wikipedia"].is<std::string>())
        cottage->wikipedia = QString::fromStdString(props["wikipedia"].get<std::string>());
    if (props["wikidata"].valid() && props["wikidata"].is<std::string>())
        cottage->wikidata = QString::fromStdString(props["wikidata"].get<std::string>());
    if (props["description"].valid() && props["description"].is<std::string>())
        cottage->description = QString::fromStdString(props["description"].get<std::string>());
    if (props["capacity"].valid() && props["capacity"].is<std::string>())
        cottage->capacity = QString::fromStdString(props["capacity"].get<std::string>());
    if (props["opening_hours"].valid() && props["opening_hours"].is<std::string>())
        cottage->opening_hours = QString::fromStdString(props["opening_hours"].get<std::string>());
    if (props["reservation"].valid() && props["reservation"].is<std::string>())
        cottage->reservation = QString::fromStdString(props["reservation"].get<std::string>());
    if (props["electricity"].valid() && props["electricity"].is<std::string>())
        cottage->electricity = QString::fromStdString(props["electricity"].get<std::string>());
    if (props["shower"].valid() && props["shower"].is<std::string>())
        cottage->shower = QString::fromStdString(props["shower"].get<std::string>());
    if (props["winter_room"].valid() && props["winter_room"].is<std::string>())
        cottage->winter_room = QString::fromStdString(props["winter_room"].get<std::string>());
    if (props["phone"].valid() && props["phone"].is<std::string>())
        cottage->phone = QString::fromStdString(props["phone"].get<std::string>());
    if (props["website"].valid() && props["website"].is<std::string>())
        cottage->website = QString::fromStdString(props["website"].get<std::string>());
    if (props["seasonal"].valid() && props["seasonal"].is<std::string>())
        cottage->seasonal = QString::fromStdString(props["seasonal"].get<std::string>());
    if (props["internet_access"].valid() && props["internet_access"].is<std::string>())
        cottage->internet_access = QString::fromStdString(props["internet_access"].get<std::string>());
    if (props["fee"].valid() && props["fee"].is<std::string>())
        cottage->fee = QString::fromStdString(props["fee"].get<std::string>());
    if (props["addr_city"].valid() && props["addr_city"].is<std::string>())
        cottage->addr_city = QString::fromStdString(props["addr_city"].get<std::string>());
    if (props["addr_street"].valid() && props["addr_street"].is<std::string>())
        cottage->addr_street = QString::fromStdString(props["addr_street"].get<std::string>());
    if (props["addr_postcode"].valid() && props["addr_postcode"].is<std::string>())
        cottage->addr_postcode = QString::fromStdString(props["addr_postcode"].get<std::string>());
    if (props["addr_housenumber"].valid() && props["addr_housenumber"].is<std::string>())
        cottage->addr_housenumber = QString::fromStdString(props["addr_housenumber"].get<std::string>());
    if (props["operators"].valid() && props["operators"].is<std::string>())
        cottage->operators = QString::fromStdString(props["operator"].get<std::string>());
    if (props["feature_type"].valid() && props["feature_type"].is<std::string>())
        cottage->feature_type = QString::fromStdString(props["feature_type"].get<std::string>());
    if (props["access"].valid() && props["access"].is<std::string>())
        cottage->access = QString::fromStdString(props["access"].get<std::string>());
    if (props["ele"].valid() && props["ele"].is<uint64_t>())
        cottage->elevation = props["ele"].get<std::uint64_t>();

    return cottage;
}

QString FeatureTXTCottage::labelText() const { return name; }

std::shared_ptr<const FeatureTXT> FeatureTXTWebcam::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto webcam = std::make_shared<FeatureTXTWebcam>();
    FeatureTXT::parse(webcam, feature, dataquerier);

    webcam->type = FeatureType::Webcam;

    auto props = feature.getProperties();

    if (props["camera_type"].valid() && props["camera_type"].is<std::string>())
        webcam->camera_type = QString::fromStdString(props["camera_type"].get<std::string>());
    if (props["direction"].valid() && props["direction"].is<uint64_t>())
        webcam->direction = props["direction"].get<std::uint64_t>();
    if (props["surveillance_type"].valid() && props["surveillance_type"].is<std::string>())
        webcam->surveillance_type = QString::fromStdString(props["surveillance_type"].get<std::string>());
    if (props["image"].valid() && props["image"].is<std::string>())
        webcam->image = QString::fromStdString(props["image"].get<std::string>());
    if (props["description"].valid() && props["description"].is<std::string>())
        webcam->description = QString::fromStdString(props["description"].get<std::string>());
    if (props["ele"].valid() && props["ele"].is<uint64_t>())
        webcam->elevation = props["ele"].get<std::uint64_t>();

    return webcam;
}

QString FeatureTXTWebcam::labelText() const { return name; }

} // namespace nucleus::vectortile

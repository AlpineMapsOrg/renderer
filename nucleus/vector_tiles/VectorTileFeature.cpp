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

std::string FeatureTXT::get_data_attribute(std::string key, std::string data)
{
    const std::regex reg{".*\\{" + key + ",(.+?)\\}.*"};

    std::smatch m;

    if(std::regex_match(data, m, reg))
    {
        return std::string(m[1].str());
    }
    else
    {
        return std::string("");
    }
}

std::map<std::string, std::string> FeatureTXT::parse_data(std::string )// data)
{
    std::map<std::string, std::string> attribute_map;

           // TODO finish

    return attribute_map;
}

void FeatureTXT::parse(std::shared_ptr<FeatureTXT> ft, const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    ft->id = feature.getID().get<uint64_t>();

    auto props = feature.getProperties();
    ft->name = QString::fromStdString(props["name"].get<std::string>());
    ft->position = glm::dvec2(props["lat"].get<double>(), props["long"].get<double>());
    if (props["data"].valid())
        ft->data = props["data"].get<std::string>();

    double altitude = 0;
    if (dataquerier)
        altitude = dataquerier->get_altitude(ft->position);

    ft->worldposition = nucleus::srs::lat_long_alt_to_world(glm::dvec3(ft->position.x, ft->position.y, altitude));
}

std::shared_ptr<FeatureTXT> FeatureTXTPeak::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto peak = std::make_shared<FeatureTXTPeak>();
    FeatureTXT::parse(peak, feature, dataquerier);

    peak->type = FeatureType::Peak;

    const auto elevation = get_data_attribute("ele", peak->data);
    peak->elevation = std::stoi(elevation);

    return peak;
}

QString FeatureTXTPeak::labelText()
{
    if (elevation == 0)
        return name;
    else
        return QString("%1 (%2m)").arg(name).arg(double(elevation), 0, 'f', 0);
}

std::shared_ptr<FeatureTXT> FeatureTXTCity::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto city = std::make_shared<FeatureTXTCity>();
    FeatureTXT::parse(city, feature, dataquerier);

    city->type = FeatureType::City;

    return city;
}

QString FeatureTXTCity::labelText()
{
    return name;
}

std::shared_ptr<FeatureTXT> FeatureTXTCottage::parse(const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    auto cottage = std::make_shared<FeatureTXTCottage>();
    FeatureTXT::parse(cottage, feature, dataquerier);

    cottage->type = FeatureType::Cottage;

    return cottage;
}

QString FeatureTXTCottage::labelText()
{
    return name;
}

} // namespace nucleus::vectortile

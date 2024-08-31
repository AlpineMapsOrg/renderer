/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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

#include "parse.h"
#include "util.h"
#include <nucleus/DataQuerier.h>

namespace {
nucleus::vector_tile::PointOfInterest::Type type_from_layer_name(const std::string& name)
{
    if (name == "Peak")
        return nucleus::vector_tile::PointOfInterest::Type::Peak;
    if (name == "cities")
        return nucleus::vector_tile::PointOfInterest::Type::Settlement;
    if (name == "cottages")
        return nucleus::vector_tile::PointOfInterest::Type::AlpineHut;
    if (name == "webcams")
        return nucleus::vector_tile::PointOfInterest::Type::Webcam;
    assert(false);
    return nucleus::vector_tile::PointOfInterest::Type::Unknown;
}

static std::atomic_int32_t s_number_of_pois = {};
} // namespace

nucleus::vector_tile::PointOfInterestCollection nucleus::vector_tile::parse::points_of_interest(
    const QByteArray& vector_tile_data, const DataQuerier* data_querier)
{
    if (vector_tile_data.isEmpty())
        return {};

    std::string d = vector_tile_data.toStdString();
    mapbox::vector_tile::buffer tile(d);

    // create empty output variable
    std::vector<PointOfInterest> pois;

    for (auto const& layer_name : tile.layerNames()) {
        const mapbox::vector_tile::layer layer = tile.getLayer(layer_name);
        const auto type = type_from_layer_name(layer_name);

        std::size_t feature_count = layer.featureCount();
        for (std::size_t i = 0; i < feature_count; ++i) {
            auto const feature = mapbox::vector_tile::feature(layer.getFeature(i), layer);
            auto props = feature.getProperties();

            PointOfInterest poi;
            poi.id = s_number_of_pois.fetch_add(1);
            poi.type = type;
            poi.name = QString::fromStdString(get<std::string>(props["name"]));
            const auto lat_long = glm::dvec2(get<double>(props["lat"]), get<double>(props["long"]));
            if (holds_alternative<double>(props["importance"]))
                poi.importance = get<double>(props["importance"]);

            double altitude = 0;
            if (data_querier)
                altitude = data_querier->get_altitude(lat_long);

            poi.lat_long_alt = glm::dvec3(lat_long.x, lat_long.y, altitude);
            poi.world_space_pos = nucleus::srs::lat_long_alt_to_world(poi.lat_long_alt);

            for (const auto& property : props) {
                const auto name = property.first;
                if (name == "name" || name == "lat" || name == "long" || name == "importance")
                    continue;
                QString value = std::visit(nucleus::vector_tile::util::string_print_visitor, property.second);
                poi.attributes[QString::fromStdString(name)] = value;
            }
            poi.attributes["id"] = QString::number(get<uint64_t>(feature.getID()));

            pois.emplace_back(poi);
        }
    }

    return pois;
}

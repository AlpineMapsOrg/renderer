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

namespace nucleus::vectortile {

void FeatureTXT::parse(std::shared_ptr<FeatureTXT> ft, const mapbox::vector_tile::feature& feature, const std::shared_ptr<DataQuerier> dataquerier)
{
    ft->id = feature.getID().get<uint64_t>();

    auto props = feature.getProperties();
    ft->name = QString::fromStdString(props["name"].get<std::string>());
    ft->position = glm::dvec2(props["lat"].get<double>(), props["long"].get<double>());

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

    auto props = feature.getProperties();

    if (props["elevation"].valid())
        peak->elevation = (int)props["elevation"].get<std::int64_t>();

    return peak;
}

QString FeatureTXTPeak::labelText()
{
    if (elevation == 0)
        return name;
    else
        return QString("%1 (%2m)").arg(name).arg(double(elevation), 0, 'f', 0);
}

} // namespace nucleus::vectortile

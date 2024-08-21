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

#include "VectorTileManager.h"

#include <mapbox/vector_tile.hpp>

#include "nucleus/DataQuerier.h"

#include "nucleus/map_label/Charset.h"

namespace nucleus::vectortile {

VectorTileManager::VectorTileManager(QObject* parent)
    : QObject { parent }
{
}


const std::shared_ptr<VectorTile> VectorTileManager::to_vector_tile(const QByteArray& vectorTileData, const std::shared_ptr<DataQuerier> dataquerier)
{
    // vectortile might be empty -> no parsing possible
    if (vectorTileData == nullptr) {
        // -> return empty
        return std::make_shared<VectorTile>();
    }

    nucleus::maplabel::Charset& charset = nucleus::maplabel::Charset::get_instance();

    if(all_chars.size() == 0)
    {
        // first time -> we should initialize all_chars list
        all_chars = charset.get_all_chars();
    }

    // convert data buffer into vectortile
    std::string d = vectorTileData.toStdString();
    mapbox::vector_tile::buffer tile(d);

    // create empty output variable
    std::shared_ptr<VectorTile> vector_tile = std::make_shared<VectorTile>();

    for (auto const& layerName : tile.layerNames()) {
        if (feature_types_factory.contains(layerName)) {
            const mapbox::vector_tile::layer layer = tile.getLayer(layerName);

            std::size_t feature_count = layer.featureCount();
            for (std::size_t i = 0; i < feature_count; ++i) {

                auto const feature = mapbox::vector_tile::feature(layer.getFeature(i), layer);

                // create the feature with the designated parser method
                auto feat = feature_types_factory.at(layerName)(feature, dataquerier);

                auto u16Chars = feat->name.toStdU16String();
                all_chars.insert(u16Chars.begin(), u16Chars.end());
                vector_tile->insert(feat);
            }
        }
    }

    if(charset.is_update_necessary(all_chars.size()))
    {
        charset.add_chars(all_chars);
    }

    return vector_tile;
}

} // namespace nucleus::vectortile

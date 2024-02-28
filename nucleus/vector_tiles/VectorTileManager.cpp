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

#include <fstream>
#include <iostream>

#include <nucleus/srs.h>

#include <mapbox/vector_tile.hpp>

namespace nucleus {

VectorTileManager::VectorTileManager(QObject* parent, DataQuerier* data_querier)
    : QObject { parent }
    , m_data_querier(data_querier)
{
}

std::unordered_set<std::shared_ptr<FeatureTXT>> VectorTileManager::get_tile(tile::Id id)
{
    if (m_loaded_tiles.contains(id)) {
        return m_loaded_tiles[id];
    } // else -> doesnt exist yet -> do nothing

    return empty;
}

/*
 *  This is the slot where the loaded tile data is parsed.
 *  This slot is connected in the controller (similar to the other tile data)
 *  The data is parsed separately to the height and ortho data, since a completely other render pass manages the data
 *  -> therefore there is no need to send assemble/send this data to the shader in a render pass where it is not needed and has to be calculated separately
 */
void VectorTileManager::deliver_vectortile(const nucleus::tile_scheduler::tile_types::TileLayer& tile)
{
    if (!m_loaded_tiles.contains(tile.id) && tile.network_info.status == nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good) {
        create_tile_data(tile);
    } // else -> already loaded once -> do nothing
}

void VectorTileManager::prepare_vector_tile(const tile::Id id)
{
    m_height_data_available.insert(id);

    if (m_loaded_tiles.contains(id)) {
        // vector tiles have already finished parsing the data
        // continue calculating the height
        calculated_label_height(id);
    }
}

void VectorTileManager::create_tile_data(const nucleus::tile_scheduler::tile_types::TileLayer& tileLayer)
{
    const auto tile_bounds = nucleus::srs::tile_bounds(tileLayer.id);

    // convert data buffer into vectortile
    std::string d = (*tileLayer.data).toStdString();
    mapbox::vector_tile::buffer tile(d);

    // set of all features in this tile
    std::unordered_set<std::shared_ptr<FeatureTXT>> features = std::unordered_set<std::shared_ptr<FeatureTXT>>();

    for (auto const& layerName : tile.layerNames()) {
        if (FEATURE_TYPES_FACTORY.contains(layerName)) {
            const mapbox::vector_tile::layer layer = tile.getLayer(layerName);
            const auto extent = double(layer.getExtent());
            // std::cout << extent << std::endl;

            std::size_t feature_count = layer.featureCount();
            for (std::size_t i = 0; i < feature_count; ++i) {

                auto const feature = mapbox::vector_tile::feature(layer.getFeature(i), layer);

                // check if feature was loaded previously and abort
                const unsigned long id = feature.getID().get<uint64_t>();
                if (m_loaded_features.contains(id)) {
                    continue; // feature is already loaded
                }

                // create the feature with the designated parser method
                const std::shared_ptr<FeatureTXT> feat = FEATURE_TYPES_FACTORY.at(layerName)(feature, tile_bounds, extent);

                m_loaded_features[id] = feat;
                features.insert(feat);
            }
        }
    }

    // insert all the features into the map with the tile id as key
    m_loaded_tiles.insert({ tileLayer.id, std::move(features) });

    if (m_height_data_available.contains(tileLayer.id)) {
        // height data has already finished loading
        // -> continue determining the actual height of the labels
        calculated_label_height(tileLayer.id);
    }
}

void VectorTileManager::calculated_label_height(const tile::Id& id)
{
    if (m_data_querier == nullptr)
        return;

    for (auto feat : m_loaded_tiles[id]) {
        feat->calculateHeight(id.zoom_level, m_data_querier);
    }

    emit vector_tile_ready(id, m_loaded_tiles[id]);
}

} // namespace nucleus

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

namespace nucleus::vectortile {

VectorTileManager::VectorTileManager(QObject* parent)
    : QObject { parent }
{
}

const std::shared_ptr<VectorTile> VectorTileManager::to_vector_tile(
    tile::Id id, const QByteArray& vectorTileData, const std::shared_ptr<DataQuerier> dataquerier)
{
    // vectortile might be empty -> no parsing possible
    if (vectorTileData == nullptr) {
        // try to find a parent tile and return previously parsed parent tile
        if (id.zoom_level > max_zoom)
            id = get_suitable_id(id);

        if (!m_loaded_tiles.contains(id)) // tile id not found -> create empty
            m_loaded_tiles[id] = std::make_shared<VectorTile>();

        return m_loaded_tiles.at(id);
    }

    // convert data buffer into vectortile
    std::string d = vectorTileData.toStdString();
    mapbox::vector_tile::buffer tile(d);

    std::shared_ptr<VectorTile> vector_tile;
    if (!m_loaded_tiles.contains(id))
        m_loaded_tiles[id] = std::make_shared<VectorTile>();
    vector_tile = m_loaded_tiles.at(id);

    for (auto const& layerName : tile.layerNames()) {
        if (FEATURE_TYPES_FACTORY.contains(layerName)) {
            const mapbox::vector_tile::layer layer = tile.getLayer(layerName);

            auto features = std::unordered_set<std::shared_ptr<FeatureTXT>>();

            std::size_t feature_count = layer.featureCount();
            for (std::size_t i = 0; i < feature_count; ++i) {

                auto const feature = mapbox::vector_tile::feature(layer.getFeature(i), layer);

                // check if feature was loaded previously and abort
                const unsigned long id = feature.getID().get<uint64_t>();
                if (m_loaded_features.contains(id)) {
                    const auto feat = m_loaded_features.at(id);
                    // feat->updateWorldPosition(dataquerier);
                    features.insert(feat);
                    continue; // feature is already loaded
                }

                // create the feature with the designated parser method
                const std::shared_ptr<FeatureTXT> feat = FEATURE_TYPES_FACTORY.at(layerName)(feature, dataquerier);
                // feat->updateWorldPosition(dataquerier);

                m_loaded_features[id] = feat;
                features.insert(feat);
            }

            vector_tile->insert(std::make_pair(FEATURE_TYPES.at(layerName), features));
        }
    }

    m_loaded_tiles[id] = vector_tile;

    return vector_tile;
}

const tile::Id VectorTileManager::get_suitable_id(tile::Id id)
{
    while (id.zoom_level > max_zoom) {
        id = id.parent();
    }
    return id;
}

} // namespace nucleus::vectortile

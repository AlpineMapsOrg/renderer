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

VectorTileManager::VectorTileManager(QObject* parent)
    : QObject { parent }
{
}

std::shared_ptr<std::unordered_set<std::shared_ptr<FeatureTXT>>> VectorTileManager::get_tile(tile::Id id)
{
    // std::cout << id << std::endl;
    if (loaded_tiles.contains(id)) {
        return loaded_tiles[id];
    } // else -> doesnt exist yet -> do nothing

    return empty;
}

void VectorTileManager::deliver_vectortile(const nucleus::tile_scheduler::tile_types::TileLayer& tile)
{
    if (!loaded_tiles.contains(tile.id) && tile.network_info.status == nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good) {
        create_tile_data(tile);
    } // else -> already loaded once -> do nothing
}

void VectorTileManager::create_tile_data(const nucleus::tile_scheduler::tile_types::TileLayer& tileLayer)
{
    const auto tile_bounds = nucleus::srs::tile_bounds(tileLayer.id);

    // convert data buffer into vectortile
    std::string d = (*tileLayer.data).toStdString();
    mapbox::vector_tile::buffer tile(d);

    std::shared_ptr<std::unordered_set<std::shared_ptr<FeatureTXT>>> features = std::make_shared<std::unordered_set<std::shared_ptr<FeatureTXT>>>();

    for (auto const& layerName : tile.layerNames()) {
        if (layerName.starts_with("Peak")) {
            const mapbox::vector_tile::layer layer = tile.getLayer(layerName);
            const auto extent = double(layer.getExtent());
            // std::cout << extent << std::endl;

            std::size_t feature_count = layer.featureCount();
            for (std::size_t i = 0; i < feature_count; ++i) {

                auto const feature = mapbox::vector_tile::feature(layer.getFeature(i), layer);

                mapbox::vector_tile::points_arrays_type geom = feature.getGeometries<mapbox::vector_tile::points_arrays_type>(1.0);
                glm::vec2 pos;
                for (auto const& point_array : geom) {
                    for (auto const& point : point_array) {
                        pos = tile_bounds.min + glm::dvec2(point.x / extent, point.y / extent) * (tile_bounds.max - tile_bounds.min);
                    }
                }

                std::shared_ptr<FeatureTXT> peak;
                long id = feature.getID().get<uint64_t>();
                if (loaded_peaks.contains(id)) {
                    continue; // peak is already loaded
                    // peak = loaded_peaks[id];
                } else {
                    peak = std::make_shared<FeatureTXT>();
                    peak->id = id;
                    peak->position = glm::vec3(pos.x, pos.y, -1000);
                }

                auto props = feature.getProperties();
                for (auto const& prop : props) {
                    if (prop.first.starts_with("name")) {
                        peak->name = prop.second.get<std::string>();
                    } else if (prop.first.starts_with("elevation")) {
                        peak->elevation = (int)prop.second.get<int64_t>();
                    }
                }

                features->insert(peak);
            }
        }
    }

    loaded_tiles.insert({ tileLayer.id, features });
}

} // namespace nucleus

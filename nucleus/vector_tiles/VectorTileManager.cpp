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

VectorTileManager::VectorTileManager()
{
}

void VectorTileManager::get_tile(tile::Id id)
{

    std::cout << "get vector tile: " << id << std::endl;

    if (!loaded_tiles.contains(id)) {
        load_tile(id);
    } // else -> already loaded once -> do nothing

}

void VectorTileManager::update_peak(std::string name, float altitude, glm::vec2 position, int zoom_level) // TODO importance (probably calculated from first visible zoom_level, but not sure if we aren't skipping some zoom_levels when loading..)
{
    auto const id = Peak::get_id(name, altitude);

    // insert or update tile
    if (loaded_peaks.contains(id)) {
        auto existing_peak = loaded_peaks[id];

        if (existing_peak->highest_zoom < zoom_level) {
            // update info since we have more precision
            existing_peak->highest_zoom = zoom_level;
            existing_peak->position = position;
        }

    } else {
        // insert the new peak
        loaded_peaks.insert(std::make_pair(id, std::make_shared<Peak>(name, altitude, position, 1.0, zoom_level)));
    }
}

void VectorTileManager::load_tile(tile::Id id)
{
    std::cout << "load vector tile: " << id << std::endl;

    std::string data = "";
    // update_tile_data(data); //TODO enable
}

void VectorTileManager::update_tile_data(std::string data, tile::Id id)
{
    const auto tile_bounds = nucleus::srs::tile_bounds(id);

    mapbox::vector_tile::buffer tile(data);

    for (auto const& name : tile.layerNames()) {
        if (name.starts_with("GIPFEL")) {
            const mapbox::vector_tile::layer layer = tile.getLayer(name);
            const auto extent = double(layer.getExtent());

            std::size_t feature_count = layer.featureCount();
            for (std::size_t i = 0; i < feature_count; ++i) {

                auto const feature = mapbox::vector_tile::feature(layer.getFeature(i), layer);

                mapbox::vector_tile::points_arrays_type geom = feature.getGeometries<mapbox::vector_tile::points_arrays_type>(1.0);
                glm::vec2 pos;
                for (auto const& point_array : geom) {
                    for (auto const& point : point_array) {
                        pos = tile_bounds.min + glm::dvec2(point.x / extent, point.y / extent) * (tile_bounds.max - tile_bounds.min);
                        // y coordinate must be flipped to get the correct positioning
                        pos.y *= -1.0;
                    }
                }

                auto props = feature.getProperties();
                for (auto const& prop : props) {

                    if (prop.first.starts_with("_name")) {

                        const auto peak_data = prop.second.get<std::string>();
                        const auto split_index = peak_data.find("\n");
                        const auto name = peak_data.substr(0, split_index);
                        const auto altitude = std::stof(peak_data.substr(split_index + 1, peak_data.size() - split_index - 2));

                        update_peak(name, altitude, pos, id.zoom_level);

                        break; // we found the name property -> we can exit the properties list
                    }
                }
            }
        }
    }
}

const std::unordered_map<size_t, std::shared_ptr<Peak>>& VectorTileManager::get_peaks() const
{
    return loaded_peaks;
}

} // namespace nucleus

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

#include "MapLabelFilter.h"

#include <iostream>

namespace nucleus::maplabel {


void MapLabelFilter::add_tile(const tile::Id id, std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>> all_features)
{
    all_tiles.insert(id);
    tiles_to_filter.push(id);
    m_all_features[id] = std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>>();

    for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END; i++) {
        nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

        if(all_features.contains(type))
            m_all_features.at(id)[type] = all_features[type];
    }
}

void MapLabelFilter::remove_tile(const tile::Id id)
{
    if(all_tiles.contains(id))
    {
        if(m_all_features.contains(id))
            m_all_features.erase(id);

        all_tiles.erase(id);
    }
}

std::unordered_map<tile::Id, std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>>, tile::Id::Hasher> MapLabelFilter::filter()
{
    auto filtered_features = std::unordered_map<tile::Id, std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>>, tile::Id::Hasher>();

    while(!tiles_to_filter.empty())
    {
        auto tile_id = tiles_to_filter.front();
        tiles_to_filter.pop();
        if(!all_tiles.contains(tile_id))
            continue; // tile was removed in the mean time

        filtered_features[tile_id] = std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>>();

        for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END; i++) {
            nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

            if(!m_all_features.contains(tile_id) || !m_all_features[tile_id].contains(type))
                continue;

            for(auto feature : m_all_features.at(tile_id)[type])
            {
                if(type == nucleus::vectortile::FeatureType::Peak)
                {
//                    std::shared_ptr<nucleus::vectortile::FeatureTXTPeak> peak_feature = std::dynamic_pointer_cast<nucleus::vectortile::FeatureTXTPeak>(feature);
//                    if(peak_feature->elevation > 3600)
                        filtered_features.at(tile_id)[type].insert(feature);

                }
                else
                {
                    filtered_features.at(tile_id)[type].insert(feature);
                }
            }
        }
    }

    return filtered_features;
}

} // namespace nucleus::maplabel

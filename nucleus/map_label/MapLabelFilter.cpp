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

MapLabelFilter::MapLabelFilter(QObject* parent)
    : QObject { parent }
{
    // DEBUG
//        m_definitions.m_peak_ele_range_filtered = true;
//        m_definitions.m_peak_ele_range = QVector2D(3500,3800);

//        m_definitions.m_cities_visible = false;
//        m_definitions.m_cottages_visible = false;
    // DEBUG

    update_filter(m_definitions);
}

void MapLabelFilter::update_filter(const FilterDefinitions& filter_definitions)
{
    m_definitions = filter_definitions;

    for(auto id : all_tiles)
        tiles_to_filter.push(id);
}
void MapLabelFilter::add_tile(const tile::Id id, nucleus::vectortile::VectorTile all_features)
{
    all_tiles.insert(id);
    tiles_to_filter.push(id);
    m_all_features[id] = nucleus::vectortile::VectorTile();

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

void MapLabelFilter::apply_filter(
    std::unordered_set<std::shared_ptr<const nucleus::vectortile::FeatureTXT>>& features, nucleus::vectortile::VectorTile& visible_features)
{
    for (auto& feature : features) {

        if (feature->type == nucleus::vectortile::FeatureType::Peak) {
            auto peak_feature = std::dynamic_pointer_cast<const nucleus::vectortile::FeatureTXTPeak>(feature);

            if (!m_definitions.m_peaks_visible) {
                continue;
            }

            if (m_definitions.m_peak_ele_range_filtered) {
                if (peak_feature->elevation < m_definitions.m_peak_ele_range.x() || peak_feature->elevation > m_definitions.m_peak_ele_range.y()) {
                    continue;
                }
            }

            // all filters were passed -> it is visible
            visible_features[nucleus::vectortile::FeatureType::Peak].insert(feature);
        } else if (feature->type == nucleus::vectortile::FeatureType::City) {
            // auto city_feature = std::dynamic_pointer_cast<const nucleus::vectortile::FeatureTXTCity>(feature);
            if (!m_definitions.m_cities_visible) {
                continue;
            }

            // all filters were passed -> it is visible
            visible_features[nucleus::vectortile::FeatureType::City].insert(feature);

        } else if (feature->type == nucleus::vectortile::FeatureType::Cottage) {
            // auto cottage_feature = std::dynamic_pointer_cast<const nucleus::vectortile::FeatureTXTCottage>(feature);
            if (!m_definitions.m_cottages_visible) {
                continue;
            }

            // all filters were passed -> it is visible
            visible_features[nucleus::vectortile::FeatureType::Cottage].insert(feature);
        }

        // TODO define other filter
    }
}

std::unordered_map<tile::Id, nucleus::vectortile::VectorTile, tile::Id::Hasher> MapLabelFilter::filter()
{
    auto visible_features = std::unordered_map<tile::Id, nucleus::vectortile::VectorTile, tile::Id::Hasher>();

    while (!tiles_to_filter.empty()) {
        auto tile_id = tiles_to_filter.front();
        tiles_to_filter.pop();
        if (!all_tiles.contains(tile_id))
            continue; // tile was removed in the mean time

        visible_features[tile_id] = nucleus::vectortile::VectorTile();
        for (int i = 0; i < nucleus::vectortile::FeatureType::ENUM_END; i++) {
            nucleus::vectortile::FeatureType type = (nucleus::vectortile::FeatureType)i;

            if (!m_all_features.contains(tile_id) || !m_all_features[tile_id].contains(type))
                continue;

            apply_filter(m_all_features.at(tile_id)[type], visible_features.at(tile_id));
        }
    }

    return visible_features;
}

} // namespace nucleus::maplabel

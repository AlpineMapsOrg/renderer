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
}

void MapLabelFilter::update_filter(const FilterDefinitions& filter_definitions)
{
    m_definitions = filter_definitions;

    for (auto id : m_all_tiles)
        m_tiles_to_filter.push(id);

    filter();
}

void MapLabelFilter::update_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    m_removed_tiles.clear();

    for (const auto& quad : new_quads) {
        for (const auto& tile : quad.tiles) {
            // test for validity
            if (!tile.vector_tile || tile.vector_tile->empty())
                continue;

            assert(tile.id.zoom_level < 100);

            if (m_all_tiles.contains(tile.id))
                continue; // no need to add it twice

            add_tile(tile.id, *tile.vector_tile);
        }
    }
    for (const auto& quad : deleted_quads) {
        for (const auto& id : quad.children()) {
            remove_tile(id);
        }
    }

    filter();
}

void MapLabelFilter::add_tile(const tile::Id id, const VectorTile& all_features)
{
    m_all_tiles.insert(id);
    m_tiles_to_filter.push(id);
    m_all_features[id] = VectorTile();

    for (int i = 0; i < FeatureType::ENUM_END; i++) {
        FeatureType type = (FeatureType)i;

        if (all_features.contains(type))
            m_all_features.at(id)[type] = all_features.at(type);
    }
}

void MapLabelFilter::remove_tile(const tile::Id id)
{
    m_removed_tiles.insert(id);
    if (m_all_tiles.contains(id)) {
        if(m_all_features.contains(id))
            m_all_features.erase(id);

        if (m_visible_features.contains(id))
            m_visible_features.erase(id);

        m_all_tiles.erase(id);
    }
}

void MapLabelFilter::apply_filter(const tile::Id tile_id, FeatureType type)
{
    for (auto& feature : m_all_features.at(tile_id)[type]) {

        if (feature->type == FeatureType::Peak) {
            auto peak_feature = std::dynamic_pointer_cast<const FeatureTXTPeak>(feature);

            if (!m_definitions.m_peaks_visible) {
                continue;
            }

            if (m_definitions.m_peak_ele_range_filtered) {
                if (peak_feature->elevation < m_definitions.m_peak_ele_range.x() || peak_feature->elevation > m_definitions.m_peak_ele_range.y()) {
                    continue;
                }
            }

            // all filters were passed -> it is visible
            m_visible_features.at(tile_id)[FeatureType::Peak].insert(feature);
        } else if (feature->type == FeatureType::City) {
            // auto city_feature = std::dynamic_pointer_cast<const FeatureTXTCity>(feature);
            if (!m_definitions.m_cities_visible) {
                continue;
            }

            // all filters were passed -> it is visible
            m_visible_features.at(tile_id)[FeatureType::City].insert(feature);

        } else if (feature->type == FeatureType::Cottage) {
            // auto cottage_feature = std::dynamic_pointer_cast<const FeatureTXTCottage>(feature);
            if (!m_definitions.m_cottages_visible) {
                continue;
            }

            // all filters were passed -> it is visible
            m_visible_features.at(tile_id)[FeatureType::Cottage].insert(feature);
        }

        // TODO define other filter
    }
}

void MapLabelFilter::filter()
{
    while (!m_tiles_to_filter.empty()) {
        auto tile_id = m_tiles_to_filter.front();
        m_tiles_to_filter.pop();
        if (!m_all_tiles.contains(tile_id))
            continue; // tile was removed in the meantime

        if (m_visible_features.contains(tile_id))
            m_visible_features.erase(tile_id); // tile was added previously -> but we want to recalculate

        m_visible_features[tile_id] = VectorTile();

        for (int i = 0; i < FeatureType::ENUM_END; i++) {
            FeatureType type = (FeatureType)i;

            if (!m_all_features.contains(tile_id) || !m_all_features[tile_id].contains(type))
                continue;

            apply_filter(tile_id, type);
        }
    }

    emit filter_finished(m_visible_features, m_removed_tiles);
}

} // namespace nucleus::maplabel

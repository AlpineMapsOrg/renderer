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
//    std::cout << "filter updated 2" << std::endl;
    m_definitions = filter_definitions;

    m_filter_peaks.clear();
    m_filter_cities.clear();
    m_filter_cottages.clear();

    // all the filter methods defined return true if a POI should be filtered/removed
    if(m_definitions.m_peaks_visible)
    {
        if(m_definitions.m_peak_ele_range_filtered)
        {
            m_filter_peaks.push_back([](const FilterDefinitions& definition, const std::shared_ptr<nucleus::vectortile::FeatureTXTPeak> feature){ return feature->elevation < definition.m_peak_ele_range.x() || feature->elevation > definition.m_peak_ele_range.y(); });
        }
    }
    else
    {
        // feature not visible
       m_filter_peaks.push_back([](const FilterDefinitions&, const std::shared_ptr<nucleus::vectortile::FeatureTXTPeak>){ return true; });
    }

    if(m_definitions.m_cities_visible)
    {
        // TODO fill
    }
    else
    {
      // feature not visible
       m_filter_cities.push_back([](const FilterDefinitions&, const std::shared_ptr<nucleus::vectortile::FeatureTXTCity>){ return true; });
    }

    if(m_definitions.m_cottages_visible)
    {
        // TODO fill
    }
    else
    {
      // feature not visible
       m_filter_cottages.push_back([](const FilterDefinitions&, const std::shared_ptr<nucleus::vectortile::FeatureTXTCottage>){ return true; });
    }

    for(auto id : all_tiles)
        tiles_to_filter.push(id);
}

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

void MapLabelFilter::apply_filter_peaks(std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>& features, std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>>& filtered_features)
{
    for(auto& feature : features)
    {
        auto specific_feature = std::dynamic_pointer_cast<nucleus::vectortile::FeatureTXTPeak>(feature);
        bool visible = true;
        for(auto filter : m_filter_peaks)
        {
            if(filter(m_definitions, specific_feature))
            {
                visible = false;
                break;
            }
        }

        if(visible)
        {
            filtered_features[nucleus::vectortile::FeatureType::Peak].insert(feature);
        }
    }
}

void MapLabelFilter::apply_filter_cities(std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>& features, std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>>& filtered_features)
{
    for(auto& feature : features)
    {
        auto specific_feature = std::dynamic_pointer_cast<nucleus::vectortile::FeatureTXTCity>(feature);
        bool visible = true;
        for(auto filter : m_filter_cities)
        {
            if(filter(m_definitions, specific_feature))
            {
                visible = false;
                break;
            }
        }

        if(visible)
        {
            filtered_features[nucleus::vectortile::FeatureType::City].insert(feature);
        }
    }
}

void MapLabelFilter::apply_filter_cottages(std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>& features, std::unordered_map<nucleus::vectortile::FeatureType, std::unordered_set<std::shared_ptr<nucleus::vectortile::FeatureTXT>>>& filtered_features)
{
    for(auto& feature : features)
    {
        auto specific_feature = std::dynamic_pointer_cast<nucleus::vectortile::FeatureTXTCottage>(feature);
        bool visible = true;
        for(auto filter : m_filter_cottages)
        {
            if(filter(m_definitions, specific_feature))
            {
                visible = false;
                break;
            }
        }

        if(visible)
        {
            filtered_features[nucleus::vectortile::FeatureType::Cottage].insert(feature);
        }
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

//            for(auto feature : m_all_features.at(tile_id)[type])
//            {
            if(type == nucleus::vectortile::FeatureType::Peak)
                apply_filter_peaks(m_all_features.at(tile_id)[type], filtered_features.at(tile_id));
            else if(type == nucleus::vectortile::FeatureType::City)
                 apply_filter_cities(m_all_features.at(tile_id)[type], filtered_features.at(tile_id));
            else if(type == nucleus::vectortile::FeatureType::Cottage)
                apply_filter_cottages(m_all_features.at(tile_id)[type], filtered_features.at(tile_id));
//            }
        }
    }

    return filtered_features;
}

} // namespace nucleus::maplabel

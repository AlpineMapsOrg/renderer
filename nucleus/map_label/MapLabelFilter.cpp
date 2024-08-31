/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Adam Celarek
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

namespace nucleus::maplabel {

MapLabelFilter::MapLabelFilter(QObject* parent)
    : QObject { parent }
{
    m_update_filter_timer = std::make_unique<QTimer>(this);
    m_update_filter_timer->setSingleShot(true);

    connect(m_update_filter_timer.get(), &QTimer::timeout, this, &MapLabelFilter::filter);
}

void MapLabelFilter::update_filter(const FilterDefinitions& filter_definitions)
{
    m_definitions = filter_definitions;

    for (const auto& key_value : m_all_pois)
        m_tiles_to_filter.push(key_value.first);

    if (!m_update_filter_timer->isActive()) {
        // start timer to prevent future filter updates to happen rapidly one after another
        m_update_filter_timer->start(m_update_filter_time);
        // set bool to true to indicate that filter should code should run
        m_filter_should_run = true;
        // start the filter process
        filter();
    } else {
        // update_filter is called while the last update just happened
        // -> set bool to true to indicate that after timer runs out we want to call filter again with the latest definition
        m_filter_should_run = true;
    }
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

            if (m_all_pois.contains(tile.id))
                continue; // no need to add it twice

            add_tile(tile.id, tile.vector_tile);
        }
    }
    for (const auto& quad : deleted_quads) {
        for (const auto& id : quad.children()) {
            remove_tile(id);
        }
    }

    // update_quads should always execute the filter method
    m_filter_should_run = true;
    filter();
}

void MapLabelFilter::add_tile(const tile::Id id, const PointOfInterestCollectionPtr& all_features)
{
    m_tiles_to_filter.push(id);
    m_all_pois[id] = all_features;
}

void MapLabelFilter::remove_tile(const tile::Id id)
{
    m_removed_tiles.push_back(id);
    if (m_all_pois.contains(id))
        m_all_pois.erase(id);
}

PointOfInterestCollection MapLabelFilter::apply_filter(const PointOfInterestCollection& unfiltered_pois)
{
    PointOfInterestCollection filtered_pois;
    filtered_pois.reserve(unfiltered_pois.size());
    std::copy_if(unfiltered_pois.begin(), unfiltered_pois.end(), std::back_inserter(filtered_pois), [this](const PointOfInterest& poi) {
        if (poi.type == LabelType::Peak) {
            if (!m_definitions.m_peaks_visible)
                return false;
            if (poi.lat_long_alt.z < m_definitions.m_peak_ele_range.x() || poi.lat_long_alt.z > m_definitions.m_peak_ele_range.y())
                return false;
            if (m_definitions.m_peak_has_cross && !(poi.attributes.value("summit_cross") == "yes"))
                return false;
            if (m_definitions.m_peak_has_register && !(poi.attributes.value("summit_register") == "yes"))
                return false;
            return true;
        }
        if (poi.type == LabelType::Settlement) {
            if (!m_definitions.m_cities_visible)
                return false;
            return true;
        }
        if (poi.type == LabelType::AlpineHut) {
            if (!m_definitions.m_cottages_visible)
                return false;
            if (m_definitions.m_cottage_has_shower && !(poi.attributes.value("shower") == "yes"))
                return false;
            if (m_definitions.m_cottage_has_contact && !(poi.attributes.value("email").size() || poi.attributes.value("phone").size()))
                return false;
            return true;
        }
        if (poi.type == LabelType::Webcam) {
            if (!m_definitions.m_webcams_visible)
                return false;
            return true;
        }
        assert(false);
        return true;
    });
    return filtered_pois;
}

void MapLabelFilter::filter()
{
    // test if this filter should run or not (by checking here we prevent double running once the timer runs out)
    if (!m_filter_should_run)
        return;
    m_filter_should_run = false;

    PointOfInterestTileCollection filtered_tiles;
    while (!m_tiles_to_filter.empty()) {
        auto tile_id = m_tiles_to_filter.front();
        m_tiles_to_filter.pop();
        if (!m_all_pois.contains(tile_id))
            continue; // tile was removed in the meantime

        filtered_tiles[tile_id] = std::make_shared<PointOfInterestCollection>(apply_filter(*m_all_pois.at(tile_id)));
    }

    emit filter_finished(filtered_tiles, m_removed_tiles);
}

} // namespace nucleus::maplabel

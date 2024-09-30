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

#pragma once

#include <QObject>
#include <QTimer>
#include <QVector2D>
#include <queue>
#include <radix/tile.h>
#include <nucleus/map_label/FilterDefinitions.h>
#include <nucleus/tile_scheduler/tile_types.h>

using namespace nucleus::vector_tile;

namespace nucleus::maplabel {

class MapLabelFilter : public QObject {
    Q_OBJECT
    using LabelType = vector_tile::PointOfInterest::Type;

public:
    explicit MapLabelFilter(QObject* parent = nullptr);

    void add_tile(const tile::Id id, const PointOfInterestCollectionPtr& all_features);
    void remove_tile(const tile::Id id);

public slots:
    void update_filter(const FilterDefinitions& filter_definitions);
    void update_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);

signals:
    void filter_finished(const PointOfInterestTileCollection& visible_features, const std::vector<tile::Id>& removed_tiles);

private slots:
    void filter();

private:
    PointOfInterestTileCollection m_all_pois;
    std::queue<tile::Id> m_tiles_to_filter;
    std::vector<tile::Id> m_removed_tiles;

    FilterDefinitions m_definitions;

    PointOfInterestCollection apply_filter(const PointOfInterestCollection& unfiltered_pois);

    bool m_filter_should_run;
    constexpr static int m_update_filter_time = 400;
    std::unique_ptr<QTimer> m_update_filter_timer;
};
} // namespace nucleus::maplabel

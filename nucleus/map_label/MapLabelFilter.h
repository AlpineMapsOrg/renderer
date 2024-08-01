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

#pragma once

#include <QObject>
#include <QVector2D>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <radix/tile.h>

#include "app/LabelFilter.h"
#include "nucleus/tile_scheduler/tile_types.h"
#include "nucleus/vector_tiles/VectorTileFeature.h"

using namespace nucleus::vectortile;

namespace nucleus::maplabel {

class MapLabelFilter : public QObject {
    Q_OBJECT
public:
    explicit MapLabelFilter(QObject* parent = nullptr);

    void add_tile(const tile::Id id, const VectorTile& all_features);
    void remove_tile(const tile::Id id);

public slots:
    void update_filter(const FilterDefinitions& filter_definitions);
    void update_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);

signals:
    void filter_finished(const TiledVectorTile& visible_features, const std::unordered_set<tile::Id, tile::Id::Hasher> removed_tiles);

private:
    TiledVectorTile m_all_features;
    TiledVectorTile m_visible_features;

    std::queue<tile::Id> m_tiles_to_filter;
    std::unordered_set<tile::Id, tile::Id::Hasher> m_all_tiles;
    std::unordered_set<tile::Id, tile::Id::Hasher> m_removed_tiles;

    FilterDefinitions m_definitions;

    void apply_filter(const tile::Id tile_id, FeatureType type);
    void filter();
};
} // namespace nucleus::maplabel

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

#include "radix/tile.h"
#include <mapbox/vector_tile.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QByteArray>
#include <QObject>

#include "nucleus/tile_scheduler/tile_types.h"
#include "nucleus/vector_tiles/VectorTileFeature.h"

namespace nucleus {

class VectorTileManager : public QObject {
    Q_OBJECT
public:
    explicit VectorTileManager(QObject* parent = nullptr, DataQuerier* data_querier = nullptr);

    std::unordered_set<std::shared_ptr<FeatureTXT>> get_tile(tile::Id id);

    inline static const QString TILE_SERVER = "http://localhost:8080/austria.peaks/";

    // all individual features and an appropriate parser method are stored in the following map
    // typedef std::shared_ptr<FeatureTXT> (*FeatureTXTParser)(const mapbox::vector_tile::feature& feature, tile::SrsBounds& tile_bounds, double extent);
    typedef std::shared_ptr<FeatureTXT> (*FeatureTXTParser)(const mapbox::vector_tile::feature&, const tile::SrsBounds&, const double);
    inline static const std::unordered_map<std::string, FeatureTXTParser> FEATURE_TYPES_FACTORY = { { "Peak", FeatureTXTPeak::parse } };

public slots:
    void deliver_vectortile(const nucleus::tile_scheduler::tile_types::TileLayer& tile);
    void prepare_vector_tile(const tile::Id id);

signals:
    void vector_tile_ready(const tile::Id id, const std::unordered_set<std::shared_ptr<FeatureTXT>>& features);

private:
    const static inline std::unordered_set<std::shared_ptr<FeatureTXT>> empty
        = std::unordered_set<std::shared_ptr<FeatureTXT>>(); // default reference we return if the tile does not (yet) exist
    std::unordered_map<tile::Id, std::unordered_set<std::shared_ptr<FeatureTXT>>, tile::Id::Hasher> m_loaded_tiles;
    std::unordered_map<unsigned long, std::shared_ptr<FeatureTXT>> m_loaded_features;

    // using this set we store whether or not the height data has already finished loading
    std::unordered_set<tile::Id, tile::Id::Hasher> m_height_data_available;

    DataQuerier* m_data_querier;

    void create_tile_data(const nucleus::tile_scheduler::tile_types::TileLayer& tileLayer);

    void calculated_label_height(const tile::Id& id);
};

} // namespace nucleus

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
    explicit VectorTileManager(QObject* parent = nullptr);

    std::shared_ptr<std::unordered_set<std::shared_ptr<FeatureTXT>>> get_tile(tile::Id id);

    // const std::unordered_map<size_t, std::shared_ptr<Peak>>& get_peaks() const;

    inline static const QString TILE_SERVER = "http://localhost:8080/austria.peaks/";

public slots:
    void deliver_vectortile(const nucleus::tile_scheduler::tile_types::TileLayer& tile);

private:
    const static inline std::shared_ptr<std::unordered_set<std::shared_ptr<FeatureTXT>>> empty = std::make_shared<std::unordered_set<std::shared_ptr<FeatureTXT>>>(); // default reference we return if the tile does not (yet) exist
    std::unordered_map<tile::Id, std::shared_ptr<std::unordered_set<std::shared_ptr<FeatureTXT>>>, tile::Id::Hasher> loaded_tiles;

    void create_tile_data(const nucleus::tile_scheduler::tile_types::TileLayer& tileLayer);

    // void update_peak(std::string name, float altitude, glm::vec2 position, int zoom_level);

    // std::unordered_set<tile::Id, tile::Id::Hasher> loaded_tiles;
    std::unordered_map<long, std::shared_ptr<FeatureTXT>> loaded_peaks;
};

} // namespace nucleus

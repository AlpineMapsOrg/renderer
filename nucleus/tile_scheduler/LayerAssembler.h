/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include <unordered_map>

#include <QObject>

#include "tile_types.h"

namespace nucleus::tile_scheduler {

class LayerAssembler : public QObject {
    Q_OBJECT
    using TileId2DataMap = std::unordered_map<tile::Id, tile_types::TileLayer, tile::Id::Hasher>;

    TileId2DataMap m_ortho_data;
    TileId2DataMap m_height_data;
    TileId2DataMap m_vector_tile_data;

public:
    explicit LayerAssembler(QObject* parent = nullptr);
    [[nodiscard]] size_t n_items_in_flight() const;
    static tile_types::LayeredTile join(
        const tile_types::TileLayer& ortho_tile, const tile_types::TileLayer& height_tile, const tile_types::TileLayer& vector_tile);

public slots:
    void load(const tile::Id& tile_id);
    void deliver_ortho(const tile_types::TileLayer& tile);
    void deliver_height(const tile_types::TileLayer& tile);
    void deliver_vectortile(const tile_types::TileLayer& tile);

signals:
    void tile_requested(const tile::Id& tile_id);
    void tile_loaded(const tile_types::LayeredTile& tile);

private:
    void check_and_emit(const tile::Id& tile_id);
};

} // namespace nucleus::tile_scheduler

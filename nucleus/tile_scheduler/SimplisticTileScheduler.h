/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include "nucleus/TileScheduler.h"

class SimplisticTileScheduler : public TileScheduler {
    Q_OBJECT
public:
    SimplisticTileScheduler();

    [[nodiscard]] static std::vector<tile::Id> load_candidates(const camera::Definition& camera, const tile_scheduler::AabbDecoratorPtr& aabb_decorator);
    [[nodiscard]] size_t number_of_tiles_in_transit() const override;
    [[nodiscard]] size_t number_of_waiting_height_tiles() const override;
    [[nodiscard]] size_t number_of_waiting_ortho_tiles() const override;
    [[nodiscard]] TileSet gpu_tiles() const override;

    bool enabled() const override;
    void set_enabled(bool newEnabled) override;

public slots:
    void update_camera(const camera::Definition& camera) override;
    void receive_ortho_tile(tile::Id tile_id, std::shared_ptr<QByteArray> data) override;
    void receive_height_tile(tile::Id tile_id, std::shared_ptr<QByteArray> data) override;
    void notify_about_unavailable_ortho_tile(tile::Id tile_id) override;
    void notify_about_unavailable_height_tile(tile::Id tile_id) override;

private:
    void checkLoadedTile(const tile::Id& tile_id);
    template <typename Predicate>
    void removeGpuTileIf(Predicate condition);


    TileSet m_unavaliable_tiles;
    TileSet m_pending_tile_requests;
    TileSet m_gpu_tiles;
    Tile2DataMap m_received_ortho_tiles;
    Tile2DataMap m_received_height_tiles;
    bool m_enabled = true;
};

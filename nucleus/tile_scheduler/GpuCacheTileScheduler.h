/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek
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

#include "../TileScheduler.h"
#include <QTimer>

class GpuCacheTileScheduler : public TileScheduler
{
    Q_OBJECT

public:
    GpuCacheTileScheduler();

    [[nodiscard]] static TileSet loadCandidates(const camera::Definition& camera, const tile_scheduler::AabbDecoratorPtr& aabb_decorator);
    [[nodiscard]] size_t numberOfTilesInTransit() const override;
    [[nodiscard]] size_t numberOfWaitingHeightTiles() const override;
    [[nodiscard]] size_t numberOfWaitingOrthoTiles() const override;
    [[nodiscard]] TileSet gpuTiles() const override;

    bool enabled() const override;
    void setEnabled(bool newEnabled) override;

public slots:
    void updateCamera(const camera::Definition& camera) override;
    void receiveOrthoTile(tile::Id tile_id, std::shared_ptr<QByteArray> data) override;
    void receiveHeightTile(tile::Id tile_id, std::shared_ptr<QByteArray> data) override;
    void notifyAboutUnavailableOrthoTile(tile::Id tile_id) override;
    void notifyAboutUnavailableHeightTile(tile::Id tile_id) override;
    void print_debug_info() const override;
    void set_gpu_cache_size(unsigned);
    void set_max_n_simultaneous_requests(unsigned int new_max_n_simultaneous_requests);
    void purge_gpu_cache_from_old_tiles();
    void purge_main_cache_from_old_tiles();

private slots:
    void do_update();

private:
    bool send_to_gpu_if_available(const tile::Id& tile_id);
    void remove_gpu_tiles(const std::vector<tile::Id>& tiles);

    static constexpr unsigned m_ortho_tile_size = 256;
    static constexpr unsigned m_height_tile_size = 64;
    unsigned m_max_n_simultaneous_requests = 64;
    static constexpr unsigned m_main_cache_size = 5000;

    camera::Definition m_current_camera;
    TileSet m_pending_tile_requests;
    TileSet m_gpu_tiles;
    Tile2DataMap m_received_ortho_tiles; // used as main cache
    Tile2DataMap m_received_height_tiles; // used as main cache
    std::unordered_map<tile::Id, unsigned, tile::Id::Hasher> m_main_cache_book;

    std::shared_ptr<QByteArray> m_default_ortho_tile;
    std::shared_ptr<QByteArray> m_default_height_tile;
    QTimer m_gpu_purge_timer;
    QTimer m_main_cache_purge_timer;
    QTimer m_update_timer;
    const uint64_t m_construction_msec_since_epoch = 0;
    unsigned m_gpu_cache_size = 0;
    bool m_enabled = true;
};

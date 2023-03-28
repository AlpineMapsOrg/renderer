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

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <QObject>
#include <QTimer>

#include "nucleus/camera/Definition.h"
#include "sherpa/tile.h"

namespace nucleus {
struct Tile;
}

namespace nucleus::tile_scheduler {
class AabbDecorator;
using AabbDecoratorPtr = std::shared_ptr<AabbDecorator>;

class GpuCacheTileScheduler : public QObject {
    Q_OBJECT
public:
    using TileSet = std::unordered_set<tile::Id, tile::Id::Hasher>;
    using Tile2DataMap = std::unordered_map<tile::Id, std::shared_ptr<QByteArray>, tile::Id::Hasher>;

    GpuCacheTileScheduler();
    ~GpuCacheTileScheduler() override;

    [[nodiscard]] const tile_scheduler::AabbDecoratorPtr& aabb_decorator() const;

    [[nodiscard]] float permissible_screen_space_error() const;
    void set_permissible_screen_space_error(float new_permissible_screen_space_error);

public:
    [[nodiscard]] std::vector<tile::Id> load_candidates(const nucleus::camera::Definition& camera, const AabbDecoratorPtr& aabb_decorator) const;
    [[nodiscard]] size_t number_of_tiles_in_transit() const;
    [[nodiscard]] size_t number_of_waiting_height_tiles() const;
    [[nodiscard]] size_t number_of_waiting_ortho_tiles() const;
    [[nodiscard]] TileSet gpu_tiles() const;

    bool enabled() const;
    void set_enabled(bool newEnabled);

signals:
    void tile_requested(const tile::Id& tile_id) const;
    void tile_ready(const std::shared_ptr<Tile>& tile) const;
    void tile_expired(const tile::Id& tile_id) const;
    void debug_scheduler_stats_updated(const QString& stats) const;

public slots:
    void set_aabb_decorator(const tile_scheduler::AabbDecoratorPtr& new_aabb_decorator);
    void send_debug_scheduler_stats() const;
    void key_press(const QKeyCombination&);
    void update_camera(const nucleus::camera::Definition& camera);
    void receive_ortho_tile(const tile::Id& tile_id, const std::shared_ptr<QByteArray>& data);
    void receive_height_tile(const tile::Id& tile_id, const std::shared_ptr<QByteArray>& data);
    void notify_about_unavailable_ortho_tile(tile::Id tile_id);
    void notify_about_unavailable_height_tile(tile::Id tile_id);
    void print_debug_info() const;
    void set_gpu_cache_size(unsigned);
    void set_max_n_simultaneous_requests(unsigned int new_max_n_simultaneous_requests);
    void purge_gpu_cache_from_old_tiles();
    void purge_main_cache_from_old_tiles();

private slots:
    void do_update();

private:
    void read_disk_cache();
    void schedule_update();
    void schedule_update_and_clear_pending_request_if_available(const tile::Id& tile_id);
    bool send_to_gpu_if_available(const tile::Id& tile_id);
    void remove_gpu_tiles(const std::vector<tile::Id>& tiles);

    tile_scheduler::AabbDecoratorPtr m_aabb_decorator;
    float m_permissible_screen_space_error = 2.0;

    static constexpr unsigned m_ortho_tile_size = 256;
    static constexpr unsigned m_height_tile_size = 64;
    unsigned m_max_n_simultaneous_requests = 64;
    static constexpr unsigned m_main_cache_size = 50000;

    nucleus::camera::Definition m_current_camera;
    TileSet m_pending_tile_requests;
    TileSet m_gpu_tiles;
    TileSet m_unavailable_tiles;
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
}

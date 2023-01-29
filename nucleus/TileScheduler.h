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

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <QObject>

#include "nucleus/camera/Definition.h"
#include "sherpa/tile.h"

class QKeyEvent;
namespace nucleus::tile_scheduler {
class AabbDecorator;
using AabbDecoratorPtr = std::shared_ptr<AabbDecorator>;
}

namespace nucleus {
struct Tile;

class TileScheduler : public QObject {
    Q_OBJECT
public:
    using TileSet = std::unordered_set<tile::Id, tile::Id::Hasher>;
    using Tile2DataMap = std::unordered_map<tile::Id, std::shared_ptr<QByteArray>, tile::Id::Hasher>;
    TileScheduler();

    [[nodiscard]] virtual size_t number_of_tiles_in_transit() const = 0;
    [[nodiscard]] virtual size_t number_of_waiting_height_tiles() const = 0;
    [[nodiscard]] virtual size_t number_of_waiting_ortho_tiles() const = 0;
    [[nodiscard]] virtual TileSet gpu_tiles() const = 0;
    [[nodiscard]] virtual bool enabled() const = 0;
    [[nodiscard]] const tile_scheduler::AabbDecoratorPtr& aabb_decorator() const;

public slots:
    virtual void set_enabled(bool newEnabled) = 0;
    void set_aabb_decorator(const tile_scheduler::AabbDecoratorPtr& new_aabb_decorator);
    virtual void update_camera(const nucleus::camera::Definition& camera) = 0;
    virtual void receive_ortho_tile(tile::Id tile_id, std::shared_ptr<QByteArray> data) = 0;
    virtual void receive_height_tile(tile::Id tile_id, std::shared_ptr<QByteArray> data) = 0;
    virtual void notify_about_unavailable_ortho_tile(tile::Id tile_id) = 0;
    virtual void notify_about_unavailable_height_tile(tile::Id tile_id) = 0;
    virtual void print_debug_info() const;
    virtual void send_debug_scheduler_stats() const;
    void key_press(const QKeyCombination&);

signals:
    void tile_requested(const tile::Id& tile_id) const;
    void tile_ready(const std::shared_ptr<Tile>& tile) const;
    void tile_expired(const tile::Id& tile_id) const;
    void debug_scheduler_stats_updated(const QString& stats) const;

private:
    tile_scheduler::AabbDecoratorPtr m_aabb_decorator;
};
}

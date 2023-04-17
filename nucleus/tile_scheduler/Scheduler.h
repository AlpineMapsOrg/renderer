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

#include <memory>

#include <QObject>

#include "nucleus/camera/Definition.h"
#include "sherpa/tile.h"

#include "Cache.h"
#include "tile_types.h"

class QTimer;

namespace nucleus::tile_scheduler {
namespace utils {
    class AabbDecorator;
    using AabbDecoratorPtr = std::shared_ptr<AabbDecorator>;
}

class Scheduler : public QObject {
    Q_OBJECT
public:
    explicit Scheduler(QObject* parent = nullptr);
    ~Scheduler() override;

    void set_update_timeout(unsigned int new_update_timeout);

    [[nodiscard]] bool enabled() const;
    void set_enabled(bool new_enabled);

    void set_permissible_screen_space_error(float new_permissible_screen_space_error);

    void set_aabb_decorator(const utils::AabbDecoratorPtr& new_aabb_decorator);

    void set_gpu_quad_limit(unsigned int new_gpu_quad_limit);

    void set_ram_quad_limit(unsigned int new_ram_quad_limit);

    void set_purge_timeout(unsigned int new_purge_timeout);

    const Cache<tile_types::TileQuad>& ram_cache() const;

signals:
    void quads_requested(const std::vector<tile::Id>& id);
    void gpu_quads_updated(const std::vector<tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);

public slots:
    void update_camera(const nucleus::camera::Definition& camera);
    void receiver_quads(const std::vector<tile_types::TileQuad>& new_quads);

private slots:
    void update_gpu_quads();
    void send_quad_requests();
    void purge_ram_cache();

protected:
    void schedule_update();
    void schedule_purge();
    std::vector<tile::Id> tiles_for_current_camera_position() const;

private:
    float m_permissible_screen_space_error = 2;
    unsigned m_update_timeout = 100;
    unsigned m_purge_timeout = 1000;
    unsigned m_gpu_quad_limit = 250;
    unsigned m_ram_quad_limit = 1000;
    static constexpr unsigned m_ortho_tile_size = 256;
    static constexpr unsigned m_height_tile_size = 64;
    bool m_enabled = false;
    std::unique_ptr<QTimer> m_update_timer;
    std::unique_ptr<QTimer> m_purge_timer;
    camera::Definition m_current_camera;
    utils::AabbDecoratorPtr m_aabb_decorator;
    Cache<tile_types::TileQuad> m_ram_cache;
    Cache<tile_types::GpuCacheInfo> m_gpu_cached;
    std::shared_ptr<QByteArray> m_default_ortho_tile;
    std::shared_ptr<QByteArray> m_default_height_tile;
};

}

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

    [[nodiscard]] unsigned int update_timeout() const;
    void set_update_timeout(unsigned int new_update_timeout);

    [[nodiscard]] bool enabled() const;
    void set_enabled(bool new_enabled);

    void set_permissible_screen_space_error(float new_permissible_screen_space_error);

    void set_aabb_decorator(const utils::AabbDecoratorPtr& new_aabb_decorator);

signals:
    void quads_requested(const std::vector<tile::Id>& id);

public slots:
    void update_camera(const nucleus::camera::Definition& camera);
    void receiver_quads(const std::vector<tile_types::TileQuad>& new_quads);

private slots:
    void do_update();

protected:
    void schedule_update();
    std::vector<tile::Id> tiles_for_current_camera_position() const;

private:
    float m_permissible_screen_space_error = 2;
    unsigned m_update_timeout = 100;
    unsigned m_ortho_tile_size = 256;
    bool m_enabled = false;
    std::unique_ptr<QTimer> m_update_timer;
    camera::Definition m_current_camera;
    utils::AabbDecoratorPtr m_aabb_decorator;
    Cache<tile_types::TileQuad> m_ram_cache;
};

}

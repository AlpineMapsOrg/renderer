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

#include <QNetworkAccessManager>
#include <QObject>
#include <memory>

namespace nucleus {
class AbstractRenderWindow;
class DataQuerier;
namespace tile_scheduler {
class TileLoadService;
class Scheduler;
}
namespace camera {
class Controller;
}

class Controller : public QObject {
    Q_OBJECT
public:
    explicit Controller(AbstractRenderWindow* render_window);
    ~Controller() override;

    camera::Controller* camera_controller() const;

    tile_scheduler::Scheduler* tile_scheduler() const;

private:
    AbstractRenderWindow* m_render_window;
    QNetworkAccessManager m_network_manager;
#ifdef ALP_ENABLE_THREADING
    std::unique_ptr<QThread> m_scheduler_thread;
#endif
    std::unique_ptr<tile_scheduler::TileLoadService> m_terrain_service;
    std::unique_ptr<tile_scheduler::TileLoadService> m_ortho_service;
    std::unique_ptr<tile_scheduler::Scheduler> m_tile_scheduler;
    std::unique_ptr<DataQuerier> m_data_querier;
    std::unique_ptr<camera::Controller> m_camera_controller;
};
}

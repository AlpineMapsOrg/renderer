/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include <QThread>
#include <memory>

#include "nucleus/tile/SchedulerDirector.h"
#include "nucleus/tile/setup.h"
#include "webgpu/webgpu.h"

namespace webgpu_engine {
class Context;
}

namespace nucleus {
class DataQuerier;
}

namespace nucleus::tile {
class GeometryScheduler;
class TextureScheduler;
class SchedulerDirector;
} // namespace nucleus::tile

namespace nucleus::tile::utils {
class AabbDecorator;
}

namespace webgpu_app {

class RenderingContext : public QObject {
    Q_OBJECT
public:
    RenderingContext();

    void initialize(WGPUInstance webgpu_instance, WGPUDevice webgpu_device);
    void destroy();

    webgpu_engine::Context* engine_context();
    nucleus::tile::utils::AabbDecorator* aabb_decorator();
    nucleus::DataQuerier* data_querier();
    nucleus::tile::GeometryScheduler* geometry_scheduler();
    nucleus::tile::TileLoadService* geometry_tile_load_service();
    nucleus::tile::TextureScheduler* ortho_scheduler();
    nucleus::tile::SchedulerDirector* scheduler_director();
    nucleus::tile::TileLoadService* ortho_tile_load_service();

signals:
    void initialised();

private:
    std::unique_ptr<webgpu_engine::Context> m_engine_context;

    std::shared_ptr<nucleus::tile::utils::AabbDecorator> m_aabb_decorator;
    std::shared_ptr<nucleus::DataQuerier> m_data_querier;
    nucleus::tile::setup::GeometrySchedulerHolder m_geometry_scheduler_holder;
    nucleus::tile::setup::TextureSchedulerHolder m_ortho_scheduler_holder;
    std::unique_ptr<nucleus::tile::SchedulerDirector> m_scheduler_director;

    std::unique_ptr<QThread> m_scheduler_thread;
};

} // namespace webgpu_app

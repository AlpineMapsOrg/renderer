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

#include "Controller.h"

#include <QCoreApplication>
#include <QFile>
#include <QNetworkReply>
#ifdef ALP_ENABLE_THREADING
#include <QThread>
#endif

#include "AbstractRenderWindow.h"
#include "nucleus/TileLoadService.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/camera/NearPlaneAdjuster.h"
#include "nucleus/camera/stored_positions.h"
#include "nucleus/tile_scheduler/GpuCacheTileScheduler.h"
#include "nucleus/tile_scheduler/SimplisticTileScheduler.h"
#include "nucleus/tile_scheduler/utils.h"
#include "sherpa/TileHeights.h"

namespace nucleus {
Controller::Controller(AbstractRenderWindow* render_window)
    : m_render_window(render_window)
{
    qRegisterMetaType<nucleus::event_parameter::Touch>();
    qRegisterMetaType<nucleus::event_parameter::Mouse>();
    qRegisterMetaType<nucleus::event_parameter::Wheel>();

    m_camera_controller = std::make_unique<nucleus::camera::Controller>(nucleus::camera::stored_positions::westl_hochgrubach_spitze(), m_render_window->depth_tester());
    //    nucleus::camera::Controller camera_controller { nucleus::camera::stored_positions::stephansdom() };

    m_terrain_service = std::make_unique<TileLoadService>("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TileLoadService::UrlPattern::ZXY, ".png");
    //    m_ortho_service.reset(new TileLoadService("https://tiles.bergfex.at/styles/bergfex-osm/", TileLoadService::UrlPattern::ZXY_yPointingSouth, ".jpeg"));
    m_ortho_service.reset(new TileLoadService("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg"));
    //    m_ortho_service.reset(new TileLoadService(
    //        "https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg", { "", "1", "2", "3", "4" }));

    m_tile_scheduler = std::make_unique<nucleus::tile_scheduler::GpuCacheTileScheduler>();
    m_tile_scheduler->set_gpu_cache_size(1000);

    {
        QFile file(":/resources/height_data.atb");
        const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
        assert(open);
        const QByteArray data = file.readAll();
        const auto decorator = nucleus::tile_scheduler::AabbDecorator::make(TileHeights::deserialise(data));
        m_tile_scheduler->set_aabb_decorator(decorator);
        m_render_window->set_aabb_decorator(decorator);
    }

    m_near_plane_adjuster = std::make_unique<nucleus::camera::NearPlaneAdjuster>();

#ifdef ALP_ENABLE_THREADING
    m_scheduler_thread = std::make_unique<QThread>();
    m_terrain_service->moveToThread(m_scheduler_thread.get());
    m_ortho_service->moveToThread(m_scheduler_thread.get());
    m_tile_scheduler->moveToThread(m_scheduler_thread.get());
    m_scheduler_thread->start();
#endif
    //    connect(m_render_window, &AbstractRenderWindow::viewport_changed, m_camera_controller.get(), &nucleus::camera::Controller::set_viewport);
    //    connect(m_render_window, &AbstractRenderWindow::mouse_moved, m_camera_controller.get(), &nucleus::camera::Controller::mouse_move);
    //    connect(m_render_window, &AbstractRenderWindow::mouse_pressed, m_camera_controller.get(), &nucleus::camera::Controller::mouse_press);
    //    connect(m_render_window, &AbstractRenderWindow::wheel_turned, m_camera_controller.get(), &nucleus::camera::Controller::wheel_turn);
    connect(m_render_window, &AbstractRenderWindow::key_pressed, m_camera_controller.get(), &nucleus::camera::Controller::key_press);
    connect(m_render_window, &AbstractRenderWindow::key_released, m_camera_controller.get(), &nucleus::camera::Controller::key_release);
    //    connect(m_render_window, &AbstractRenderWindow::touch_made, m_camera_controller.get(), &nucleus::camera::Controller::touch);
    connect(m_render_window, &AbstractRenderWindow::key_pressed, m_tile_scheduler.get(), &TileScheduler::key_press);

    // NOTICE ME!!!! READ THIS, IF YOU HAVE TROUBLES WITH SIGNALS NOT REACHING THE QML RENDERING THREAD!!!!111elevenone
    // In Qt 6.4 and earlier the rendering thread goes to sleep. See RenderThreadNotifier.
    // At the time of writing, an additional connection from tile_ready and tile_expired to the notifier is made.
    // this only works if ALP_ENABLE_THREADING is on, i.e., the tile scheduler is on an extra thread. -> potential issue on webassembly
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_tile_scheduler.get(), &TileScheduler::update_camera);
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_near_plane_adjuster.get(), &nucleus::camera::NearPlaneAdjuster::update_camera);
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_render_window, &AbstractRenderWindow::update_camera);

    connect(m_tile_scheduler.get(), &TileScheduler::tile_requested, m_terrain_service.get(), &TileLoadService::load);
    connect(m_tile_scheduler.get(), &TileScheduler::tile_requested, m_ortho_service.get(), &TileLoadService::load);
    connect(m_tile_scheduler.get(), &TileScheduler::tile_ready, m_render_window, &AbstractRenderWindow::add_tile);
    connect(m_tile_scheduler.get(), &TileScheduler::tile_ready, m_near_plane_adjuster.get(), &nucleus::camera::NearPlaneAdjuster::add_tile);
    connect(m_tile_scheduler.get(), &TileScheduler::tile_ready, m_render_window, &AbstractRenderWindow::update_requested);
    connect(m_tile_scheduler.get(), &TileScheduler::tile_expired, m_render_window, &AbstractRenderWindow::remove_tile);
    connect(m_tile_scheduler.get(), &TileScheduler::tile_expired, m_near_plane_adjuster.get(), &nucleus::camera::NearPlaneAdjuster::remove_tile);
    connect(m_tile_scheduler.get(), &TileScheduler::debug_scheduler_stats_updated, m_render_window, &AbstractRenderWindow::update_debug_scheduler_stats);

    connect(m_ortho_service.get(), &TileLoadService::load_ready, m_tile_scheduler.get(), &TileScheduler::receive_ortho_tile);
    connect(m_ortho_service.get(), &TileLoadService::tile_unavailable, m_tile_scheduler.get(), &TileScheduler::notify_about_unavailable_ortho_tile);
    connect(m_terrain_service.get(), &TileLoadService::load_ready, m_tile_scheduler.get(), &TileScheduler::receive_height_tile);
    connect(m_terrain_service.get(), &TileLoadService::tile_unavailable, m_tile_scheduler.get(), &TileScheduler::notify_about_unavailable_height_tile);

    connect(m_near_plane_adjuster.get(), &nucleus::camera::NearPlaneAdjuster::near_plane_changed, m_camera_controller.get(), &nucleus::camera::Controller::set_near_plane);

    m_camera_controller->update();
}

Controller::~Controller()
{
#ifdef ALP_ENABLE_THREADING
    m_scheduler_thread->quit();
    m_scheduler_thread->wait(500); // msec
#endif
};

camera::Controller* Controller::camera_controller() const
{
    return m_camera_controller.get();
}

tile_scheduler::GpuCacheTileScheduler* Controller::tile_scheduler() const
{
    return m_tile_scheduler.get();
}
}

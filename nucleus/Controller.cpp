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
#include <QNetworkReply>

#include "AbstractRenderWindow.h"
#include "nucleus/TileLoadService.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/camera/CrapyInteraction.h"
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
    m_terrain_service = std::make_unique<TileLoadService>("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TileLoadService::UrlPattern::ZXY, ".png");
    //    TileLoadService ortho_service("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg");
    m_ortho_service.reset(new TileLoadService(
        "https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg", { "", "1", "2", "3", "4" }));

    m_tile_scheduler = std::make_unique<nucleus::tile_scheduler::GpuCacheTileScheduler>();
    m_tile_scheduler->set_gpu_cache_size(1000);

    QNetworkReply* reply = m_network_manager.get(QNetworkRequest(QUrl("https://gataki.cg.tuwien.ac.at/tiles/alpine_png2/height_data.atb")));
    //    QNetworkReply* reply = m_network_manager.get(QNetworkRequest(QUrl("https://alpinemaps.cg.tuwien.ac.at/threaded//height_data.atb")));
    connect(reply, &QNetworkReply::finished, this, [reply, this]() {
        const auto url = reply->url();
        const auto error = reply->error();
        if (error == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            const auto decorator = nucleus::tile_scheduler::AabbDecorator::make(TileHeights::deserialise(data));
            QTimer::singleShot(1, this, [this, decorator]() { m_tile_scheduler->set_aabb_decorator(decorator); });

            m_render_window->set_aabb_decorator(decorator);
        } else {
            qDebug() << "Loading of " << url << " failed: " << error;
            QCoreApplication::exit(0);
            // do we need better error handling?
        }
        reply->deleteLater();
    });

    m_camera_controller = std::make_unique<nucleus::camera::Controller>(nucleus::camera::stored_positions::westl_hochgrubach_spitze());
    //    nucleus::camera::Controller camera_controller { nucleus::camera::stored_positions::stephansdom() };
    m_camera_controller->set_interaction_style(std::make_unique<nucleus::camera::CrapyInteraction>());

    m_near_plane_adjuster = std::make_unique<nucleus::camera::NearPlaneAdjuster>();

#ifdef ALP_ENABLE_THREADING
    QThread scheduler_thread;
    //    m_terrain_service->moveToThread(&scheduler_thread);
    //    m_ortho_service->moveToThread(&scheduler_thread);
    m_scheduler->moveToThread(&scheduler_thread);
    scheduler_thread.start();
#endif

    connect(m_render_window, &AbstractRenderWindow::viewport_changed, m_camera_controller.get(), &nucleus::camera::Controller::set_viewport);
    connect(m_render_window, &AbstractRenderWindow::mouse_moved, m_camera_controller.get(), &nucleus::camera::Controller::mouse_move);
    connect(m_render_window, &AbstractRenderWindow::mouse_pressed, m_camera_controller.get(), &nucleus::camera::Controller::mouse_press);
    connect(m_render_window, &AbstractRenderWindow::wheel_turned, m_camera_controller.get(), &nucleus::camera::Controller::wheel_turn);
    connect(m_render_window, &AbstractRenderWindow::key_pressed, m_camera_controller.get(), &nucleus::camera::Controller::key_press);
    connect(m_render_window, &AbstractRenderWindow::touch_made, m_camera_controller.get(), &nucleus::camera::Controller::touch);
    connect(m_render_window, &AbstractRenderWindow::key_pressed, m_tile_scheduler.get(), &TileScheduler::key_press);

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
}

Controller::~Controller() = default;

camera::Controller* Controller::camera_controller() const
{
    return m_camera_controller.get();
}

}

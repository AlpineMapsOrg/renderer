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
#include "DataQuerier.h"

#include <QCoreApplication>
#include <QFile>
#include <QNetworkReply>
#ifdef ALP_ENABLE_THREADING
#include <QThread>
#endif

#include "AbstractRenderWindow.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/camera/PositionStorage.h"
#include "nucleus/tile_scheduler/LayerAssembler.h"
#include "nucleus/tile_scheduler/QuadAssembler.h"
#include "nucleus/tile_scheduler/RateLimiter.h"
#include "nucleus/tile_scheduler/Scheduler.h"
#include "nucleus/tile_scheduler/SlotLimiter.h"
#include "nucleus/tile_scheduler/TileLoadService.h"
#include "nucleus/tile_scheduler/utils.h"
#include "radix/TileHeights.h"

using namespace nucleus::tile_scheduler;

namespace nucleus {
Controller::Controller(AbstractRenderWindow* render_window)
    : m_render_window(render_window)
{
    qRegisterMetaType<nucleus::event_parameter::Touch>();
    qRegisterMetaType<nucleus::event_parameter::Mouse>();
    qRegisterMetaType<nucleus::event_parameter::Wheel>();

    m_terrain_service = std::make_unique<TileLoadService>("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TileLoadService::UrlPattern::ZXY, ".png");
    //    m_ortho_service.reset(new TileLoadService("https://tiles.bergfex.at/styles/bergfex-osm/", TileLoadService::UrlPattern::ZXY_yPointingSouth, ".jpeg"));
    //    m_ortho_service.reset(new TileLoadService("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg"));
    //    m_ortho_service.reset(new TileLoadService(
    //        "https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg", { "", "1", "2", "3", "4" }));
    m_ortho_service.reset(new TileLoadService(
        "https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg"));

    m_tile_scheduler = std::make_unique<nucleus::tile_scheduler::Scheduler>();
    m_tile_scheduler->read_disk_cache();
    m_tile_scheduler->set_gpu_quad_limit(500);
    m_tile_scheduler->set_ram_quad_limit(12000);
    {
        QFile file(":/map/height_data.atb");
        const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
        assert(open);
        Q_UNUSED(open);
        const QByteArray data = file.readAll();
        const auto decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(TileHeights::deserialise(data));
        m_tile_scheduler->set_aabb_decorator(decorator);
        m_render_window->set_aabb_decorator(decorator);
    }
    m_data_querier = std::make_unique<DataQuerier>(&m_tile_scheduler->ram_cache());
    m_camera_controller = std::make_unique<nucleus::camera::Controller>(
        nucleus::camera::PositionStorage::instance()->get("grossglockner"),
        m_render_window->depth_tester(),
        m_data_querier.get());
    {
        auto* sch = m_tile_scheduler.get();
        SlotLimiter* sl = new SlotLimiter(sch);
        RateLimiter* rl = new RateLimiter(sch);
        QuadAssembler* qa = new QuadAssembler(sch);
        LayerAssembler* la = new LayerAssembler(sch);
        connect(sch, &Scheduler::quads_requested, sl, &SlotLimiter::request_quads);
        connect(sl, &SlotLimiter::quad_requested, rl, &RateLimiter::request_quad);
        connect(rl, &RateLimiter::quad_requested, qa, &QuadAssembler::load);
        connect(qa, &QuadAssembler::tile_requested, la, &LayerAssembler::load);
        connect(la, &LayerAssembler::tile_requested, m_ortho_service.get(), &TileLoadService::load);
        connect(la, &LayerAssembler::tile_requested, m_terrain_service.get(), &TileLoadService::load);

        connect(m_ortho_service.get(), &TileLoadService::load_finished, la, &LayerAssembler::deliver_ortho);
        connect(m_terrain_service.get(), &TileLoadService::load_finished, la, &LayerAssembler::deliver_height);
        connect(la, &LayerAssembler::tile_loaded, qa, &QuadAssembler::deliver_tile);
        connect(qa, &QuadAssembler::quad_loaded, sl, &SlotLimiter::deliver_quad);
        connect(sl, &SlotLimiter::quad_delivered, sch, &Scheduler::receive_quad);
    }
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        m_tile_scheduler->set_network_reachability(n->reachability());
        connect(n, &QNetworkInformation::reachabilityChanged, m_tile_scheduler.get(), &Scheduler::set_network_reachability);
    }

#ifdef ALP_ENABLE_THREADING
    m_scheduler_thread = std::make_unique<QThread>();
    m_scheduler_thread->setObjectName("tile_scheduler_thread");
    qDebug() << "scheduler thread: " << m_scheduler_thread.get();
#ifdef __EMSCRIPTEN__ // make request from main thread on webassembly due to QTBUG-109396
    m_terrain_service->moveToThread(QCoreApplication::instance()->thread());
    m_ortho_service->moveToThread(QCoreApplication::instance()->thread());
#else
    m_terrain_service->moveToThread(m_scheduler_thread.get());
    m_ortho_service->moveToThread(m_scheduler_thread.get());
#endif
    m_tile_scheduler->moveToThread(m_scheduler_thread.get());
    m_scheduler_thread->start();
#endif
    connect(m_render_window, &AbstractRenderWindow::key_pressed, m_camera_controller.get(), &nucleus::camera::Controller::key_press);
    connect(m_render_window, &AbstractRenderWindow::key_released, m_camera_controller.get(), &nucleus::camera::Controller::key_release);
    connect(m_render_window, &AbstractRenderWindow::update_camera_requested, m_camera_controller.get(), &nucleus::camera::Controller::update_camera_request);
    connect(m_render_window, &AbstractRenderWindow::gpu_ready_changed, m_tile_scheduler.get(), &Scheduler::set_enabled);

    // NOTICE ME!!!! READ THIS, IF YOU HAVE TROUBLES WITH SIGNALS NOT REACHING THE QML RENDERING THREAD!!!!111elevenone
    // In Qt the rendering thread goes to sleep (at least until Qt 6.5, See RenderThreadNotifier).
    // At the time of writing, an additional connection from tile_ready and tile_expired to the notifier is made.
    // this only works if ALP_ENABLE_THREADING is on, i.e., the tile scheduler is on an extra thread. -> potential issue on webassembly
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_tile_scheduler.get(), &Scheduler::update_camera);
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_render_window, &AbstractRenderWindow::update_camera);

    connect(m_tile_scheduler.get(), &Scheduler::gpu_quads_updated, m_render_window, &AbstractRenderWindow::update_gpu_quads);
    connect(m_tile_scheduler.get(), &Scheduler::gpu_quads_updated, m_render_window, &AbstractRenderWindow::update_requested);

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

Scheduler* Controller::tile_scheduler() const
{
    return m_tile_scheduler.get();
}
}

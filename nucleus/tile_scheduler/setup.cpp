/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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

#include "setup.h"
#include "LayerAssembler.h"
#include "OldScheduler.h"
#include "QuadAssembler.h"
#include "RateLimiter.h"
#include "SlotLimiter.h"
#include "TileLoadService.h"
#include <QDebug>
#include <QThread>

namespace nucleus::tile_scheduler::setup {

MonolithicScheduler monolithic(TileLoadServicePtr terrain_service, TileLoadServicePtr ortho_service, const tile_scheduler::utils::AabbDecoratorPtr& aabb_decorator)
{
    auto scheduler = std::make_unique<nucleus::tile_scheduler::OldScheduler>();

    scheduler->read_disk_cache();
    scheduler->set_gpu_quad_limit(512);
    scheduler->set_ram_quad_limit(12000);
    scheduler->set_aabb_decorator(aabb_decorator);

    {
        auto* sch = scheduler.get();
        SlotLimiterLayered* sl = new SlotLimiterLayered(sch);
        RateLimiter* rl = new RateLimiter(sch);
        QuadAssemblerLayered* qa = new QuadAssemblerLayered(sch);
        LayerAssembler* la = new LayerAssembler(sch);

        QObject::connect(sch, &OldScheduler::quads_requested, sl, &SlotLimiterLayered::request_quads);
        QObject::connect(sl, &SlotLimiterLayered::quad_requested, rl, &RateLimiter::request_quad);
        QObject::connect(rl, &RateLimiter::quad_requested, qa, &QuadAssemblerLayered::load);
        QObject::connect(qa, &QuadAssemblerLayered::tile_requested, la, &LayerAssembler::load);
        QObject::connect(la, &LayerAssembler::tile_requested, ortho_service.get(), &TileLoadService::load);
        QObject::connect(la, &LayerAssembler::tile_requested, terrain_service.get(), &TileLoadService::load);
        QObject::connect(ortho_service.get(), &TileLoadService::load_finished, la, &LayerAssembler::deliver_ortho);
        QObject::connect(terrain_service.get(), &TileLoadService::load_finished, la, &LayerAssembler::deliver_height);
        QObject::connect(la, &LayerAssembler::tile_loaded, qa, &QuadAssemblerLayered::deliver_tile);
        QObject::connect(qa, &QuadAssemblerLayered::quad_loaded, sl, &SlotLimiterLayered::deliver_quad);
        QObject::connect(sl, &SlotLimiterLayered::quad_delivered, sch, &OldScheduler::receive_quad);
    }
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        scheduler->set_network_reachability(n->reachability());
        QObject::connect(n, &QNetworkInformation::reachabilityChanged, scheduler.get(), &OldScheduler::set_network_reachability);
    }

    std::unique_ptr<QThread> thread = {};
#ifdef ALP_ENABLE_THREADING
    thread = std::make_unique<QThread>();
    thread->setObjectName("tile_scheduler_thread");
    qDebug() << "scheduler thread: " << thread.get();
#ifdef __EMSCRIPTEN__ // make request from main thread on webassembly due to QTBUG-109396
    m_terrain_service->moveToThread(QCoreApplication::instance()->thread());
    m_ortho_service->moveToThread(QCoreApplication::instance()->thread());
    m_vectortile_service->moveToThread(QCoreApplication::instance()->thread());
#else
    terrain_service->moveToThread(thread.get());
    ortho_service->moveToThread(thread.get());
#endif
    scheduler->moveToThread(thread.get());
    thread->start();
#endif

    return { std::move(scheduler), std::move(thread), std::move(ortho_service), std::move(terrain_service) };
}

utils::AabbDecoratorPtr aabb_decorator()
{
    QFile file(":/map/height_data.atb");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    Q_UNUSED(open);
    const QByteArray data = file.readAll();
    return nucleus::tile_scheduler::utils::AabbDecorator::make(TileHeights::deserialise(data));
}

} // namespace nucleus::tile_scheduler::setup

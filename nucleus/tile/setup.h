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

#pragma once

#include "GeometryScheduler.h"
#include "QuadAssembler.h"
#include "RateLimiter.h"
#include "SlotLimiter.h"
#include "TextureScheduler.h"
#include "TileLoadService.h"
#include "utils.h"
#include <QCoreApplication>
#include <QThread>
#include <memory>
#include <nucleus/utils/thread.h>

namespace nucleus::tile::setup {

using TileLoadServicePtr = std::unique_ptr<TileLoadService>;

struct GeometrySchedulerHolder {
    std::shared_ptr<GeometryScheduler> scheduler;
    TileLoadServicePtr tile_service;
};

inline GeometrySchedulerHolder geometry_scheduler(TileLoadServicePtr tile_service, const tile::utils::AabbDecoratorPtr& aabb_decorator, QThread* thread = nullptr)
{
    auto scheduler = std::make_unique<GeometryScheduler>();
    scheduler->read_disk_cache();
    scheduler->set_gpu_quad_limit(512);
    scheduler->set_ram_quad_limit(12000);
    scheduler->set_aabb_decorator(aabb_decorator);

    {
        using nucleus::tile::QuadAssembler;
        using nucleus::tile::RateLimiter;
        using nucleus::tile::SlotLimiter;
        using nucleus::tile::TileLoadService;
        auto* sch = scheduler.get();
        auto* sl = new SlotLimiter(sch);
        auto* rl = new RateLimiter(sch);
        auto* qa = new QuadAssembler(sch);

        QObject::connect(sch, &Scheduler::quads_requested, sl, &SlotLimiter::request_quads);
        QObject::connect(sl, &SlotLimiter::quad_requested, rl, &RateLimiter::request_quad);
        QObject::connect(rl, &RateLimiter::quad_requested, qa, &QuadAssembler::load);
        QObject::connect(qa, &QuadAssembler::tile_requested, tile_service.get(), &TileLoadService::load);
        QObject::connect(tile_service.get(), &TileLoadService::load_finished, qa, &QuadAssembler::deliver_tile);

        QObject::connect(qa, &QuadAssembler::quad_loaded, sl, &SlotLimiter::deliver_quad);
        QObject::connect(sl, &SlotLimiter::quad_delivered, sch, &TextureScheduler::receive_quad);
    }
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        scheduler->set_network_reachability(n->reachability());
        QObject::connect(n, &QNetworkInformation::reachabilityChanged, scheduler.get(), &Scheduler::set_network_reachability);
    }

    Q_UNUSED(thread);
#ifdef ALP_ENABLE_THREADING
#ifdef __EMSCRIPTEN__ // make request from main thread on webassembly due to QTBUG-109396
    tile_service->moveToThread(QCoreApplication::instance()->thread());
#else
    if (thread)
        tile_service->moveToThread(thread);
#endif
    if (thread)
        scheduler->moveToThread(thread);
#endif

    return { std::move(scheduler), std::move(tile_service) };
}

struct TextureSchedulerHolder {
    std::shared_ptr<TextureScheduler> scheduler;
    TileLoadServicePtr tile_service;
};

inline TextureSchedulerHolder texture_scheduler(TileLoadServicePtr tile_service, const tile::utils::AabbDecoratorPtr& aabb_decorator, QThread* thread = nullptr)
{
    auto scheduler = std::make_unique<TextureScheduler>(256);
    scheduler->read_disk_cache();
    scheduler->set_gpu_quad_limit(512);
    scheduler->set_ram_quad_limit(12000);
    scheduler->set_aabb_decorator(aabb_decorator);

    {
        using nucleus::tile::QuadAssembler;
        using nucleus::tile::RateLimiter;
        using nucleus::tile::SlotLimiter;
        using nucleus::tile::TileLoadService;
        auto* sch = scheduler.get();
        auto* sl = new SlotLimiter(sch);
        auto* rl = new RateLimiter(sch);
        auto* qa = new QuadAssembler(sch);

        QObject::connect(sch, &Scheduler::quads_requested, sl, &SlotLimiter::request_quads);
        QObject::connect(sl, &SlotLimiter::quad_requested, rl, &RateLimiter::request_quad);
        QObject::connect(rl, &RateLimiter::quad_requested, qa, &QuadAssembler::load);
        QObject::connect(qa, &QuadAssembler::tile_requested, tile_service.get(), &TileLoadService::load);
        QObject::connect(tile_service.get(), &TileLoadService::load_finished, qa, &QuadAssembler::deliver_tile);

        QObject::connect(qa, &QuadAssembler::quad_loaded, sl, &SlotLimiter::deliver_quad);
        QObject::connect(sl, &SlotLimiter::quad_delivered, sch, &TextureScheduler::receive_quad);
    }
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        scheduler->set_network_reachability(n->reachability());
        QObject::connect(n, &QNetworkInformation::reachabilityChanged, scheduler.get(), &Scheduler::set_network_reachability);
    }

    Q_UNUSED(thread);
#ifdef ALP_ENABLE_THREADING
#ifdef __EMSCRIPTEN__ // make request from main thread on webassembly due to QTBUG-109396
    tile_service->moveToThread(QCoreApplication::instance()->thread());
#else
    if (thread)
        tile_service->moveToThread(thread);
#endif
    if (thread)
        scheduler->moveToThread(thread);
#endif

    return { std::move(scheduler), std::move(tile_service) };
}

inline utils::AabbDecoratorPtr aabb_decorator()
{
    QFile file(":/map/height_data.atb");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    Q_UNUSED(open);
    const QByteArray data = file.readAll();
    return nucleus::tile::utils::AabbDecorator::make(radix::TileHeights::deserialise(data));
}

} // namespace nucleus::tile::setup

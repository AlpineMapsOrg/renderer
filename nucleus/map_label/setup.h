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

#include "Scheduler.h"
#include <QThread>
#include <memory>
#include <nucleus/tile/QuadAssembler.h>
#include <nucleus/tile/RateLimiter.h>
#include <nucleus/tile/SlotLimiter.h>
#include <nucleus/tile/TileLoadService.h>

namespace nucleus::map_label::setup {

using TileLoadServicePtr = std::unique_ptr<nucleus::tile::TileLoadService>;
using DataQuerierPtr = std::shared_ptr<nucleus::DataQuerier>;

struct SchedulerHolder {
    std::shared_ptr<map_label::Scheduler> scheduler;
    TileLoadServicePtr tile_service;
};

SchedulerHolder scheduler(TileLoadServicePtr tile_service, const tile::utils::AabbDecoratorPtr& aabb_decorator, const DataQuerierPtr& data_querier, QThread* thread = nullptr)
{
    Scheduler::Settings settings;
    settings.max_zoom_level = 18;
    settings.tile_resolution = 256;
    settings.gpu_quad_limit = 512;
    auto scheduler = std::make_unique<nucleus::map_label::Scheduler>(settings);
    scheduler->set_aabb_decorator(aabb_decorator);
    scheduler->set_dataquerier(data_querier);

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
        QObject::connect(sl, &SlotLimiter::quad_delivered, sch, &nucleus::map_label::Scheduler::receive_quad);
    }
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        scheduler->set_network_reachability(n->reachability());
        QObject::connect(n, &QNetworkInformation::reachabilityChanged, scheduler.get(), &Scheduler::set_network_reachability);
    }

    Q_UNUSED(thread);
#ifdef ALP_ENABLE_THREADING
    if (thread)
        tile_service->moveToThread(thread);
    if (thread)
        scheduler->moveToThread(thread);
#endif

    return { std::move(scheduler), std::move(tile_service) };
}
} // namespace nucleus::map_label::setup

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

#include "OldScheduler.h"
#include "TileLoadService.h"
#include "utils.h"
#include <QThread>
#include <memory>
#include <nucleus/utils/thread.h>

namespace nucleus::tile_scheduler::setup {

using TileLoadServicePtr = std::unique_ptr<TileLoadService>;

struct MonolithicScheduler {
    std::unique_ptr<OldScheduler> scheduler;
    std::unique_ptr<QThread> thread;
    TileLoadServicePtr terrain_service;
    TileLoadServicePtr ortho_service;

    ~MonolithicScheduler()
    {
        nucleus::utils::thread::sync_call(scheduler.get(), [this]() { scheduler.reset(); });
        nucleus::utils::thread::sync_call(ortho_service.get(), [this]() { ortho_service.reset(); });
        nucleus::utils::thread::sync_call(terrain_service.get(), [this]() { terrain_service.reset(); });
        thread->quit();
        thread->wait(500);
    }
};

MonolithicScheduler monolithic(TileLoadServicePtr terrain_service, TileLoadServicePtr ortho_service, const tile_scheduler::utils::AabbDecoratorPtr& aabb_decorator);

utils::AabbDecoratorPtr aabb_decorator();

} // namespace nucleus::tile_scheduler::setup

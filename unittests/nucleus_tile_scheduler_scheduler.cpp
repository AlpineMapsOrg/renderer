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

#include "nucleus/tile_scheduler/Scheduler.h"

#include <catch2/catch.hpp>

#include <QSignalSpy>
#include <QThread>

#include "nucleus/camera/stored_positions.h"
#include "nucleus/tile_scheduler/tile_types.h"
#include "nucleus/tile_scheduler/utils.h"

#include <sherpa/TileHeights.h>

namespace {
std::unique_ptr<nucleus::tile_scheduler::Scheduler> default_scheduler()
{
    auto scheduler = std::make_unique<nucleus::tile_scheduler::Scheduler>();
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    scheduler->set_aabb_decorator(nucleus::tile_scheduler::utils::AabbDecorator::make(std::move(h)));
    scheduler->set_enabled(true);
    scheduler->set_update_timeout(1);
    return scheduler;
}
std::unique_ptr<nucleus::tile_scheduler::Scheduler> scheduler_with_aabb()
{
    auto scheduler = std::make_unique<nucleus::tile_scheduler::Scheduler>();
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    scheduler->set_aabb_decorator(nucleus::tile_scheduler::utils::AabbDecorator::make(std::move(h)));
    return scheduler;
}
}

TEST_CASE("nucleus/tile_scheduler/Scheduler")
{
    SECTION("enable / disable tile scheduler")
    {
        auto scheduler = scheduler_with_aabb();
        // disabled by default (enable by signal, in order to prevent a race with opengl)
        scheduler->set_update_timeout(1);

        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        CHECK(spy.empty());

        scheduler->set_enabled(true);
        spy.wait(2);
        CHECK(!spy.empty());
    }

    SECTION("update timeout & camera updates are collected")
    {
        auto scheduler = default_scheduler();
        scheduler->set_enabled(true);
        scheduler->set_update_timeout(5);

        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        scheduler->update_camera(nucleus::camera::stored_positions::westl_hochgrubach_spitze());
        spy.wait(2);
        CHECK(spy.empty());

        spy.wait(5);
        CHECK(spy.size() == 1);
    }

    SECTION("timer is not restarted on camera updates && stops if nothing happens")
    {
        auto scheduler = default_scheduler();
        scheduler->set_update_timeout(6);
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        scheduler->update_camera(nucleus::camera::stored_positions::westl_hochgrubach_spitze());
        spy.wait(2);
        CHECK(spy.empty());
        scheduler->update_camera(nucleus::camera::stored_positions::grossglockner());
        spy.wait(3);
        CHECK(spy.size() == 1);
        spy.wait(7);
        CHECK(spy.size() == 1);
    }

    SECTION("quads are being requested")
    {
        auto scheduler = default_scheduler();
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto quads = spy.front().front().value<std::vector<tile::Id>>();
        REQUIRE(quads.size() >= 5);
        // high level tiles that contain stephansdom
        // according to https://www.maptiler.com/google-maps-coordinates-tile-bounds-projection/#4/6.45/50.74
        // at the time of writing:
        // the corresponding tile list generation code is tested only by visual inspection (of the rendered scene).
        // seems to work. the following are sanity checks.
        CHECK(std::ranges::find(quads, tile::Id { 0, { 0, 0 } }) != quads.end());
        CHECK(std::ranges::find(quads, tile::Id { 1, { 1, 1 } }) != quads.end());
        CHECK(std::ranges::find(quads, tile::Id { 2, { 2, 2 } }) != quads.end());
        CHECK(std::ranges::find(quads, tile::Id { 3, { 4, 5 } }) != quads.end());
        CHECK(std::ranges::find(quads, tile::Id { 4, { 8, 10 } }) != quads.end());
    }

    SECTION("delivered quads are not requested again")
    {
        auto scheduler = default_scheduler();
        scheduler->receiver_quads({
            nucleus::tile_scheduler::tile_types::TileQuad { tile::Id { 0, { 0, 0 } } },
            nucleus::tile_scheduler::tile_types::TileQuad { tile::Id { 1, { 1, 1 } } },
            nucleus::tile_scheduler::tile_types::TileQuad { tile::Id { 2, { 2, 2 } } },
        });
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto quads = spy.front().front().value<std::vector<tile::Id>>();
        REQUIRE(quads.size() >= 5);
        // high level tiles that contain stephansdom
        // according to https://www.maptiler.com/google-maps-coordinates-tile-bounds-projection/#4/6.45/50.74
        // at the time of writing:
        // the corresponding tile list generation code is tested only by visual inspection (of the rendered scene).
        // seems to work. the following are sanity checks.
        CHECK(std::ranges::find(quads, tile::Id { 0, { 0, 0 } }) == quads.end());
        CHECK(std::ranges::find(quads, tile::Id { 1, { 1, 1 } }) == quads.end());
        CHECK(std::ranges::find(quads, tile::Id { 2, { 2, 2 } }) == quads.end());
        CHECK(std::ranges::find(quads, tile::Id { 3, { 4, 5 } }) != quads.end());
        CHECK(std::ranges::find(quads, tile::Id { 4, { 8, 10 } }) != quads.end());
    }
}

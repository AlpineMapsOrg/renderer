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

#include <catch2/catch.hpp>

#include <QSignalSpy>

#include "nucleus/tile_scheduler/RateLimiter.h"
#include "unittests/test_helpers.h"

TEST_CASE("nucleus/tile_scheduler/rate limiter")
{
    using namespace nucleus::tile_scheduler;
    SECTION("tile requests are forwarded")
    {
        RateLimiter rl;
        QSignalSpy spy(&rl, &RateLimiter::quad_requested);
        rl.request_quad(tile::Id { 0, { 0, 0 } });
        REQUIRE(spy.size() == 1);
        CHECK(spy[0][0].value<tile::Id>() == tile::Id { 0, { 0, 0 } });
        rl.request_quad(tile::Id { 1, { 0, 0 } });
        REQUIRE(spy.size() == 2);
        CHECK(spy[1][0].value<tile::Id>() == tile::Id { 1, { 0, 0 } });
    }

    SECTION("sends on requests only up to the limit of tile slots")
    {
        RateLimiter rl;
        rl.set_limit(2, 3);
        QSignalSpy spy(&rl, &RateLimiter::quad_requested);
        rl.request_quad(tile::Id { 0, { 0, 0 } });
        rl.request_quad(tile::Id { 1, { 0, 0 } });
        rl.request_quad(tile::Id { 2, { 0, 0 } });
        REQUIRE(spy.size() == 2);
        CHECK(spy[0][0].value<tile::Id>() == tile::Id { 0, { 0, 0 } });
        CHECK(spy[1][0].value<tile::Id>() == tile::Id { 1, { 0, 0 } });
    }

    SECTION("slots are freed up after some time and request queue is processed")
    {
        qDebug("slots are freed up after some time and request queue is processed 1");
        RateLimiter rl;
        rl.set_limit(2, 3);
        QSignalSpy spy(&rl, &RateLimiter::quad_requested);
        rl.request_quad(tile::Id { 0, { 0, 0 } });
        rl.request_quad(tile::Id { 1, { 0, 0 } });
        rl.request_quad(tile::Id { 2, { 0, 0 } });
        rl.request_quad(tile::Id { 3, { 0, 0 } });
        rl.request_quad(tile::Id { 4, { 0, 0 } });
        rl.request_quad(tile::Id { 5, { 0, 0 } });
        REQUIRE(spy.size() == 2);
        qDebug("slots are freed up after some time and request queue is processed 2");
        test_helpers::process_events_for(4);
        qDebug("slots are freed up after some time and request queue is processed 3");
        REQUIRE(spy.size() == 4);
        CHECK(spy[2][0].value<tile::Id>() == tile::Id { 2, { 0, 0 } });
        CHECK(spy[3][0].value<tile::Id>() == tile::Id { 3, { 0, 0 } });
        test_helpers::process_events_for(3);
        qDebug("slots are freed up after some time and request queue is processed 4");
        REQUIRE(spy.size() == 6);
        CHECK(spy[4][0].value<tile::Id>() == tile::Id { 4, { 0, 0 } });
        CHECK(spy[5][0].value<tile::Id>() == tile::Id { 5, { 0, 0 } });
    }
}

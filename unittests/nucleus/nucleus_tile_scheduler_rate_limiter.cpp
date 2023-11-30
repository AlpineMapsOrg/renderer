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

#include <random>

#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>

#include "RateTester.h"
#include "nucleus/tile_scheduler/RateLimiter.h"
#include "test_helpers.h"

#ifdef __EMSCRIPTEN__
constexpr auto timing_multiplicator = 200;
#elif defined __ANDROID__
constexpr auto timing_multiplicator = 50;
#elif defined _MSC_VER
constexpr auto timing_multiplicator = 50;
#else
constexpr auto timing_multiplicator = 10;
#endif

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
        rl.set_limit(2, 3 * timing_multiplicator);
        unittests::RateTester tester(&rl);
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
        RateLimiter rl;
        rl.set_limit(2, 4 * timing_multiplicator);
        unittests::RateTester tester(&rl);
        QSignalSpy spy(&rl, &RateLimiter::quad_requested);
        rl.request_quad(tile::Id { 0, { 0, 0 } });
        rl.request_quad(tile::Id { 1, { 0, 0 } });
        rl.request_quad(tile::Id { 2, { 0, 0 } });
        rl.request_quad(tile::Id { 3, { 0, 0 } });
        rl.request_quad(tile::Id { 4, { 0, 0 } });
        rl.request_quad(tile::Id { 5, { 0, 0 } });
        REQUIRE(spy.size() == 2);
        test_helpers::process_events_for(6 * timing_multiplicator);
        REQUIRE(spy.size() == 4);
        test_helpers::process_events_for(4 * timing_multiplicator);
        REQUIRE(spy.size() == 6);

        for (int i = 0; i < spy.size(); ++i)
            CHECK(spy[i][0].value<tile::Id>() == tile::Id { unsigned(i), { 0, 0 } });
    }

    SECTION("request queue is handled correctly, when requests come in one after the other")
    {
        {
            RateLimiter rl;
            rl.set_limit(2, 4 * timing_multiplicator);
            unittests::RateTester tester(&rl);
            QSignalSpy spy(&rl, &RateLimiter::quad_requested);
            rl.request_quad(tile::Id { 0, { 0, 0 } });
            rl.request_quad(tile::Id { 1, { 0, 0 } });

            rl.request_quad(tile::Id { 2, { 0, 0 } });
            CHECK(spy.size() == 2);
            test_helpers::process_events_for(6 * timing_multiplicator);
            CHECK(spy.size() == 3);
            rl.request_quad(tile::Id { 3, { 0, 0 } });
            CHECK(spy.size() == 4);
            rl.request_quad(tile::Id { 4, { 0, 0 } });
            rl.request_quad(tile::Id { 5, { 0, 0 } });
            test_helpers::process_events_for(5 * timing_multiplicator);
            CHECK(spy.size() == 6);

            for (int i = 0; i < spy.size(); ++i)
                CHECK(spy[i][0].value<tile::Id>() == tile::Id { unsigned(i), { 0, 0 } });
        }
        {
            RateLimiter rl;
            rl.set_limit(2, 10 * timing_multiplicator);
            unittests::RateTester tester(&rl);
            QSignalSpy spy(&rl, &RateLimiter::quad_requested);
            rl.request_quad(tile::Id { 0, { 0, 0 } }); // sent at t=0
            rl.request_quad(tile::Id { 1, { 0, 0 } }); // sent at t=0
            rl.request_quad(tile::Id { 2, { 0, 0 } }); // sent at t=11
            CHECK(spy.size() == 2);
            test_helpers::process_events_for(12 * timing_multiplicator);
            CHECK(spy.size() == 3);
            rl.request_quad(tile::Id { 3, { 0, 0 } }); // sent at t=12
            CHECK(spy.size() == 4);
            rl.request_quad(tile::Id { 4, { 0, 0 } }); // sent at t=22
            rl.request_quad(tile::Id { 5, { 0, 0 } }); // sent at t=23
            CHECK(spy.size() == 4);
            test_helpers::process_events_for(12 * timing_multiplicator);
            rl.request_quad(tile::Id { 6, { 0, 0 } });
            rl.request_quad(tile::Id { 7, { 0, 0 } });
            rl.request_quad(tile::Id { 8, { 0, 0 } });
            CHECK(spy.size() == 6);

            for (int i = 0; i < spy.size(); ++i)
                CHECK(spy[i][0].value<tile::Id>() == tile::Id { unsigned(i), { 0, 0 } });
        }
        {
            RateLimiter rl;
            rl.set_limit(2, 10 * timing_multiplicator);
            unittests::RateTester tester(&rl);
            unsigned request_no = 0;
            const auto make_request = [&rl, &request_no]() {
                rl.request_quad(tile::Id { request_no++, { 0, 0 } });
            };
            QSignalSpy spy(&rl, &RateLimiter::quad_requested);
            make_request();
            CHECK(spy.size() == 1);
            test_helpers::process_events_for(2 * timing_multiplicator);

            make_request();
            CHECK(spy.size() == 2);
            test_helpers::process_events_for(2 * timing_multiplicator);

            make_request();
            CHECK(spy.size() == 2);
            test_helpers::process_events_for(2 * timing_multiplicator);

            make_request();
            CHECK(spy.size() == 2);
            test_helpers::process_events_for(2 * timing_multiplicator);

            make_request();
            CHECK(spy.size() == 2);
            test_helpers::process_events_for(3 * timing_multiplicator);
            CHECK(spy.size() == 3);

            make_request();
            make_request();
            test_helpers::process_events_for(2 * timing_multiplicator);
            CHECK(spy.size() == 4);

            test_helpers::process_events_for(30 * timing_multiplicator);
            CHECK(unsigned(spy.size()) == request_no);

            for (int i = 0; i < spy.size(); ++i)
                CHECK(spy[i][0].value<tile::Id>() == tile::Id { unsigned(i), { 0, 0 } });
        }
    }
    SECTION("fuzzy load test")
    {
        std::mt19937 mt(42);
        const auto test_for = [&mt](unsigned rate, unsigned period) {
            RateLimiter rl;
            rl.set_limit(rate, period * timing_multiplicator);
            unittests::RateTester tester(&rl);
            unsigned request_no = 0;
            const auto make_request = [&rl, &request_no]() {
                rl.request_quad(tile::Id { request_no++, { 0, 0 } });
            };
            QSignalSpy spy(&rl, &RateLimiter::quad_requested);

//            unsigned n_requests = 0;
//            unsigned processing_time = 0;
            for (unsigned iteration = 0; iteration < 200; ++iteration) {
                const auto rnd_request_count = std::uniform_int_distribution<unsigned>(1, rate)(mt);
//                n_requests += rnd_request_count;
                for (unsigned i = 0; i < rnd_request_count; ++i) {
                    make_request();
                }
                auto rnd_processing_time = std::uniform_int_distribution<unsigned>(0, period)(mt);
//                processing_time += rnd_processing_time;
                test_helpers::process_events_for(rnd_processing_time);
                if (std::uniform_real_distribution<float>(0.0f, 1.0f)(mt) < 0.10f) {
                    while (rl.queue_size()) {
//                        processing_time += period;
                        test_helpers::process_events_for(period);
                    }
                }
            }
            //            qDebug("rate: %d, period: %d, max r/p: %f, requests: %d in %dmsecs (%f r/msec)",
            //                rate, period, float(rate) / period, n_requests, processing_time, float(n_requests) / processing_time);

            for (int i = 0; i < spy.size(); ++i)
                CHECK(spy[i][0].value<tile::Id>() == tile::Id { unsigned(i), { 0, 0 } });
        };

        for (const unsigned rate : { 1u, /*2, 3, 5,*/ 7u /*, 13*/ }) {
            for (const unsigned period : { 1u /*, 2, 3*/, 5u /*, 7*/ }) {
                test_for(rate, period);
            }
        }
    }
}

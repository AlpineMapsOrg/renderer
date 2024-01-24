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

#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "RateTester.h"

using namespace unittests;

RateTester::RateTester(nucleus::tile_scheduler::RateLimiter* rate_limiter)
    : m_rate(rate_limiter->limit().first)
    , m_period(rate_limiter->limit().second)
{
    connect(rate_limiter, &nucleus::tile_scheduler::RateLimiter::quad_requested, this, &RateTester::receive_quad_request);
}

RateTester::~RateTester()
{
    for (auto i = m_events.cbegin(); i != m_events.cend(); ++i) {
        unsigned n_events_in_time_frame = 1;
        const auto event_time = *i;
        for (auto j = std::reverse_iterator<decltype(i)>(i); j != m_events.rend(); j++) {
            assert(*j <= event_time);
            if (event_time - *j < m_period * 1'000ll)
                ++n_events_in_time_frame;
            else
                break;
        }
        CHECK(n_events_in_time_frame <= m_rate);
    }
}

void RateTester::receive_quad_request(const tile::Id& id)
{
    CHECK(id.zoom_level == m_events.size());
    m_events.push_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count());
}

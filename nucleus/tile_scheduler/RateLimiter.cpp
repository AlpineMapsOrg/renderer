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

#include "RateLimiter.h"

#include <QTimer>

using namespace nucleus::tile_scheduler;

RateLimiter::RateLimiter(QObject* parent)
    : QObject { parent }
    , m_update_timer(std::make_unique<QTimer>())
{
    m_update_timer->setSingleShot(true);
    connect(m_update_timer.get(), &QTimer::timeout, this, &RateLimiter::process_request_queue);
}

RateLimiter::~RateLimiter() = default;

void RateLimiter::set_limit(unsigned int rate, unsigned int period_msecs)
{
    assert(rate < std::numeric_limits<int>::max());
    assert(period_msecs < std::numeric_limits<int>::max());
    m_rate = rate;
    m_rate_period_msecs = period_msecs;
}

void RateLimiter::request_quad(const tile::Id& id)
{
    if (m_in_flight.size() < m_rate) {
        m_in_flight.push_back(0);
        emit quad_requested(id);
    } else {
        m_request_queue.push_back(id);
        m_update_timer->start(int(m_rate_period_msecs));
    }
}

void RateLimiter::process_request_queue()
{
    unsigned in_flight = 0;
    for (const auto& id : m_request_queue) {
        emit quad_requested(id);
        if (++in_flight >= m_rate)
            break;
    }
    m_request_queue.erase(m_request_queue.cbegin(), m_request_queue.cbegin() + in_flight);
    if (!m_request_queue.empty())
        m_update_timer->start(int(m_rate_period_msecs));
}

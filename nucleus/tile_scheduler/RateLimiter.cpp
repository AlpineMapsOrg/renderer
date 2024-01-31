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

#include "utils.h"

using namespace nucleus::tile_scheduler;

RateLimiter::RateLimiter(QObject* parent)
    : QObject { parent }
    , m_update_timer(std::make_unique<QTimer>(this))
{
    m_update_timer->setSingleShot(true);
    connect(m_update_timer.get(), &QTimer::timeout, this, &RateLimiter::process_request_queue);
}

RateLimiter::~RateLimiter() = default;

void RateLimiter::set_limit(unsigned int rate, unsigned int period_msecs)
{
    assert(rate < unsigned(std::numeric_limits<int>::max()));
    assert(period_msecs < unsigned(std::numeric_limits<int>::max()));
    m_rate = rate;
    m_rate_period_msecs = period_msecs;
}

std::pair<unsigned int, unsigned int> RateLimiter::limit() const
{
    return { m_rate, m_rate_period_msecs };
}

size_t RateLimiter::queue_size() const
{
    return m_request_queue.size();
}

void RateLimiter::request_quad(const tile::Id& id)
{
    m_request_queue.push_back(id);
    process_request_queue();
}

void RateLimiter::process_request_queue()
{
    const auto current_msecs = utils::time_since_epoch();
    std::erase_if(m_in_flight, [&current_msecs, this](const auto& x) { return x < current_msecs - m_rate_period_msecs; });
    unsigned requested_now = 0;
    for (const auto& id : m_request_queue) {
        if (m_in_flight.size() >= m_rate)
            break;
        m_in_flight.push_back(current_msecs);
        ++requested_now;
        emit quad_requested(id);
    }
    m_request_queue.erase(m_request_queue.cbegin(), m_request_queue.cbegin() + requested_now);

    if (!m_request_queue.empty()) {
        const auto age_of_oldest_in_flight = current_msecs - m_in_flight.front();
        assert(age_of_oldest_in_flight <= m_rate_period_msecs);

        m_update_timer->start(int(1 + m_rate_period_msecs - age_of_oldest_in_flight));
    }
}

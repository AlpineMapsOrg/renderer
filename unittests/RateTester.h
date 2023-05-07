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

#pragma once

#include <QObject>

#include "nucleus/tile_scheduler/RateLimiter.h"

namespace unittests {

class RateTester : public QObject {
    Q_OBJECT

    std::vector<int64_t> m_events;
    unsigned m_rate;
    unsigned m_period;

public:
    explicit RateTester(nucleus::tile_scheduler::RateLimiter* period);
    ~RateTester() override;

public slots:
    void receive_quad_request(const tile::Id& id);
};
}

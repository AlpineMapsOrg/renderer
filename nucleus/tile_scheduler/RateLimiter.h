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

#include <unordered_set>

#include <QObject>

#include <radix/tile.h>

class QTimer;

namespace nucleus::tile_scheduler {

class RateLimiter : public QObject
{
    Q_OBJECT
    unsigned m_rate = 100;
    unsigned m_rate_period_msecs = 1000 * 1;
    std::vector<tile::Id> m_request_queue;
    std::vector<uint64_t> m_in_flight;
    std::unique_ptr<QTimer> m_update_timer;

public:
    explicit RateLimiter(QObject* parent = nullptr);
    ~RateLimiter() override;
    void set_limit(unsigned rate, unsigned period_msecs);
    std::pair<unsigned, unsigned> limit() const;
    size_t queue_size() const;

public slots:
    void request_quad(const tile::Id& id);

private slots:
    void process_request_queue();

signals:
    void quad_requested(const tile::Id& tile_id);
};
}

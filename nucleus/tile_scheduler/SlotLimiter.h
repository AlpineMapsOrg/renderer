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

#include "tile_types.h"

namespace nucleus::tile_scheduler {

class SlotLimiter : public QObject {
    Q_OBJECT

    unsigned m_limit = 16;
    std::unordered_set<tile::Id, tile::Id::Hasher> m_in_flight;
    std::vector<tile::Id> m_request_queue;

public:
    explicit SlotLimiter(QObject* parent = nullptr);

    void set_limit(unsigned int new_limit);
    [[nodiscard]] unsigned int limit() const;
    unsigned int slots_taken() const;

public slots:
    void request_quads(const std::vector<tile::Id>& id);
    void deliver_quad(const tile_types::TileQuad& tile);

signals:
    void quad_requested(const tile::Id& tile_id);
    void quad_delivered(const tile_types::TileQuad& id);
};

}

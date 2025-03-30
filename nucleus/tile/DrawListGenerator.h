/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2024 Patrick Komon
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

#include "radix/iterator.h"
#include "utils.h"
#include <nucleus/camera/Definition.h>
#include <unordered_set>

namespace nucleus::tile {
class DrawListGenerator
{
public:
    using TileSet = std::unordered_set<tile::Id, tile::Id::Hasher>;

    DrawListGenerator();

    void set_aabb_decorator(const utils::AabbDecoratorPtr& new_aabb_decorator);
    void add_tile(const tile::Id& id);
    void remove_tile(const tile::Id& id);
    [[nodiscard]] TileSet generate_for(const camera::Definition& camera, unsigned tile_size, unsigned max_zoom_level) const;

    template<class TileIdContainerType>
    TileSet cull(const TileIdContainerType& tileset, const camera::Frustum& frustum) const
    {
        TileSet visible_leaves;
        visible_leaves.reserve(tileset.size());

        const auto is_visible = [frustum, this](const tile::Id& tile) {
            return tile::utils::camera_frustum_contains_tile(frustum, m_aabb_decorator->aabb(tile));
        };

        std::copy_if(tileset.begin(), tileset.end(), radix::unordered_inserter(visible_leaves), is_visible);
        return visible_leaves;
    }

    const TileSet& tiles() const;

private:
    utils::AabbDecoratorPtr m_aabb_decorator;
    TileSet m_available_tiles;
};
}

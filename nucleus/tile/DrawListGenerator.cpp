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

#include "DrawListGenerator.h"

#include "radix/iterator.h"
#include "radix/quad_tree.h"

using TileSet = nucleus::tile::DrawListGenerator::TileSet;
using namespace nucleus::tile;

DrawListGenerator::DrawListGenerator()
{
    radix::TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    set_aabb_decorator(tile::utils::AabbDecorator::make(std::move(h)));
}

void DrawListGenerator::set_permissible_screen_space_error(float new_permissible_screen_space_error)
{
    m_permissible_screen_space_error = new_permissible_screen_space_error;
}

void DrawListGenerator::set_aabb_decorator(const tile::utils::AabbDecoratorPtr& new_aabb_decorator)
{
    m_aabb_decorator = new_aabb_decorator;
}

void DrawListGenerator::add_tile(const tile::Id& id)
{
    m_available_tiles.insert(id);
}

void DrawListGenerator::remove_tile(const tile::Id& id)
{
    m_available_tiles.erase(id);
}

const TileSet& DrawListGenerator::tiles() const { return m_available_tiles; }

DrawListGenerator::TileSet DrawListGenerator::generate_for(const nucleus::camera::Definition& camera) const
{
    const auto tile_refine_functor = tile::utils::refineFunctor(camera, m_aabb_decorator, m_permissible_screen_space_error);
    const auto draw_refine_functor = [&tile_refine_functor, this](const tile::Id& tile) {
        bool all = true;
        for (const auto& child : tile.children()) {
            all = all && m_available_tiles.contains(child);
        }
        return all && tile_refine_functor(tile);
    };

    const auto all_leaves = radix::quad_tree::onTheFlyTraverse(tile::Id { 0, { 0, 0 } }, draw_refine_functor, [](const tile::Id& v) { return v.children(); });

    TileSet tileset;
    tileset.reserve(all_leaves.size());
    std::copy(all_leaves.begin(), all_leaves.end(), radix::unordered_inserter(tileset));
    return tileset;
}

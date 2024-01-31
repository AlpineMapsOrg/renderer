/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
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

using nucleus::tile_scheduler::DrawListGenerator;

DrawListGenerator::DrawListGenerator()
{
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    set_aabb_decorator(tile_scheduler::utils::AabbDecorator::make(std::move(h)));
}

void DrawListGenerator::set_permissible_screen_space_error(float new_permissible_screen_space_error)
{
    m_permissible_screen_space_error = new_permissible_screen_space_error;
}

void DrawListGenerator::set_aabb_decorator(const tile_scheduler::utils::AabbDecoratorPtr& new_aabb_decorator)
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

DrawListGenerator::TileSet DrawListGenerator::generate_for(const nucleus::camera::Definition& camera) const
{
    const auto tile_refine_functor
        = tile_scheduler::utils::refineFunctor(camera,
                                               m_aabb_decorator,
                                               m_permissible_screen_space_error);
    const auto draw_refine_functor = [&tile_refine_functor, this](const tile::Id &tile) {
        bool all = true;
        for (const auto &child : tile.children()) {
            all = all && m_available_tiles.contains(child);
        }
        return all && tile_refine_functor(tile);
    };

    const auto all_leaves = quad_tree::onTheFlyTraverse(tile::Id { 0, { 0, 0 } }, draw_refine_functor, [](const tile::Id& v) { return v.children(); });
    TileSet visible_leaves;
    visible_leaves.reserve(all_leaves.size());

    const auto camera_frustum = camera.frustum();

    const auto is_visible = [camera_frustum, this](const tile::Id& tile) {
        return tile_scheduler::utils::camera_frustum_contains_tile(camera_frustum, m_aabb_decorator->aabb(tile));
    };

    std::copy_if(all_leaves.begin(), all_leaves.end(), radix::unordered_inserter(visible_leaves), is_visible);
    return visible_leaves;
}

/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adam Celarek
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

#include "drawing.h"
#include <radix/quad_tree.h>
#include <unordered_set>

namespace nucleus::tile::drawing {

std::vector<TileBounds> generate_list(const camera::Definition& camera, utils::AabbDecoratorPtr aabb_decorator, unsigned int max_zoom_level)
{
    const auto tile_refine_functor = tile::utils::refineFunctor(camera, aabb_decorator, 1, 256, max_zoom_level);

    auto tiles = radix::quad_tree::onTheFlyTraverse(tile::Id { 0, { 0, 0 } }, tile_refine_functor, [](const tile::Id& v) { return v.children(); });

    if (tiles.size() > max_n_tiles) {
        std::sort(tiles.begin(), tiles.end(), [&](const tile::Id& a, const tile::Id& b) { return a.zoom_level > b.zoom_level; });
        std::unordered_set<tile::Id, tile::Id::Hasher> id_set;
        id_set.reserve(tiles.size());
        for (const auto t : tiles) {
            id_set.insert(t);
        }
        const auto all_in_set = [&](const std::array<Id, 4>& siblings) {
            for (const auto& s : siblings) {
                if (!id_set.contains(s))
                    return false;
            }
            return true;
        };
        const auto remove = [&](const std::array<Id, 4>& siblings) {
            for (const auto& s : siblings) {
                const auto pos = std::find(tiles.crbegin(), tiles.crend(), s);
                tiles.erase(std::next(pos).base());
                id_set.erase(s);
            }
            return true;
        };

        while (tiles.size() > max_n_tiles) {
            for (auto it = tiles.crbegin(); it != tiles.crend(); ++it) {
                const auto parent = (*it).parent();
                const auto siblings = parent.children();
                if (all_in_set(siblings)) {
                    remove(siblings);
                    tiles.push_back(parent);
                    id_set.insert(parent);
                    break;
                }
            }
        }
    }

    std::vector<TileBounds> bounded_tiles;
    bounded_tiles.reserve(tiles.size());
    for (const auto& t : tiles) {
        bounded_tiles.emplace_back(t, aabb_decorator->aabb(t));
    }
    return bounded_tiles;
}

std::vector<TileBounds> sort(std::vector<TileBounds> list, const glm::dvec3& camera_position)
{
    std::sort(list.begin(), list.end(), [&](const TileBounds& a, const TileBounds& b) {
        const auto delta_a = (glm::vec2(a.bounds.min) + glm::vec2(a.bounds.max)) * 0.5f - glm::vec2(camera_position);
        const auto delta_b = (glm::vec2(b.bounds.min) + glm::vec2(b.bounds.max)) * 0.5f - glm::vec2(camera_position);
        const auto dist_a = glm::dot(delta_a, delta_a);
        const auto dist_b = glm::dot(delta_b, delta_b);

        // const auto dist_a = glm::distance(a.bounds.centre(), camera_position);
        // const auto dist_b = glm::distance(b.bounds.centre(), camera_position);
        return dist_a < dist_b;
    });
    return list;
}

} // namespace nucleus::tile::drawing

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

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/tile/drawing.h>
#include <nucleus/tile/utils.h>
#include <radix/TileHeights.h>

using namespace radix;
using namespace nucleus::tile;
using nucleus::tile::utils::AabbDecorator;

TEST_CASE("tile/drawing")
{
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    const auto aabb_decorator = AabbDecorator::make(std::move(h));
    std::vector<std::pair<nucleus::camera::Definition, std::vector<tile::Id>>> lists;
    std::vector<std::pair<nucleus::camera::Definition, std::vector<TileBounds>>> tile_bound_lists;
    SECTION("should not generate more than 1024 tiles for all test cameras + sorting")
    {
        for (auto [name, camera] : nucleus::camera::PositionStorage::instance()->positions()) {
            CAPTURE(name);
            camera.set_viewport_size({ 1920, 1080 });
            const auto list = drawing::generate_list(camera, aabb_decorator, 20);
            lists.emplace_back(camera, list);
            const auto limited = drawing::limit(list, 1024u);
            CHECK(limited.size() <= 1024u);

            const auto tiles_with_bounds = drawing::compute_bounds(limited, aabb_decorator);
            tile_bound_lists.emplace_back(camera, tiles_with_bounds);
            // qDebug() << "list.size(): " << list.size();
            const auto sorted = drawing::sort(tiles_with_bounds, camera.position());
            auto dist = 0.0;
            for (const auto t : sorted) {
                const auto d = glm::distance(t.bounds.centre(), camera.position());
                CHECK(d > dist);
            }
        }
    }

    BENCHMARK("generate list")
    {
        std::vector<std::vector<tile::Id>> tmp;
        tmp.reserve(lists.size());
        for (const auto& [camera, list] : lists) {
            tmp.push_back(drawing::generate_list(camera, aabb_decorator, 20));
        }
        return tmp;
    };

    BENCHMARK("limit")
    {
        std::vector<std::vector<tile::Id>> tmp;
        tmp.reserve(lists.size());
        for (const auto& [camera, list] : lists) {
            tmp.push_back(drawing::limit(list, 1024u));
        }
        return tmp;
    };

    BENCHMARK("compute_aabbs")
    {
        std::vector<std::vector<TileBounds>> tmp;
        tmp.reserve(lists.size());
        for (const auto& [camera, list] : lists) {
            tmp.push_back(drawing::compute_bounds(list, aabb_decorator));
        }
        return tmp;
    };

    BENCHMARK("sort")
    {
        std::vector<std::vector<TileBounds>> tmp;
        tmp.reserve(lists.size());
        for (const auto& [camera, list] : tile_bound_lists) {
            tmp.push_back(drawing::sort(list, camera.position()));
        }
        return tmp;
    };

    BENCHMARK("cull")
    {
        std::vector<std::vector<TileBounds>> tmp;
        tmp.reserve(lists.size());
        for (const auto& [camera, list] : tile_bound_lists) {
            tmp.push_back(drawing::cull(list, camera));
        }
        return tmp;
    };
}

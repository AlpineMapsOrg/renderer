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

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QFile>

#include "nucleus/camera/PositionStorage.h"
#include "nucleus/tile_scheduler/DrawListGenerator.h"
#include "nucleus/tile_scheduler/utils.h"
#include "radix/TileHeights.h"
#include "radix/quad_tree.h"

TEST_CASE("nucleus/tile_scheduler/DrawListGenerator")
{
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    auto camera = nucleus::camera::stored_positions::oestl_hochgrubach_spitze();
    camera.set_viewport_size({ 1920, 1080 });

    nucleus::tile_scheduler::DrawListGenerator draw_list_generator;
    draw_list_generator.set_aabb_decorator(nucleus::tile_scheduler::utils::AabbDecorator::make(std::move(h)));

    SECTION("root only")
    {
        draw_list_generator.add_tile(tile::Id { 0, { 0, 0 } });
        const auto list = draw_list_generator.generate_for(camera);
        REQUIRE(list.size() == 1);
        CHECK(list.contains(tile::Id { 0, { 0, 0 } }));
    }


    SECTION("tr only")
    {
        draw_list_generator.add_tile(tile::Id { 0, { 0, 0 } });

        draw_list_generator.add_tile(tile::Id { 1, { 0, 0 } });
        draw_list_generator.add_tile(tile::Id { 1, { 0, 1 } });
        draw_list_generator.add_tile(tile::Id { 1, { 1, 0 } });
        draw_list_generator.add_tile(tile::Id { 1, { 1, 1 } });
        const auto list = draw_list_generator.generate_for(camera);
        REQUIRE(list.size() == 1);
        CHECK(list.contains(tile::Id { 1, { 1, 1 } }));
    }


    SECTION("removal")
    {
        draw_list_generator.add_tile(tile::Id { 0, { 0, 0 } });

        draw_list_generator.add_tile(tile::Id { 1, { 0, 0 } });
        draw_list_generator.add_tile(tile::Id { 1, { 0, 1 } });
        draw_list_generator.add_tile(tile::Id { 1, { 1, 0 } });
        draw_list_generator.add_tile(tile::Id { 1, { 1, 1 } });

        draw_list_generator.remove_tile(tile::Id { 1, { 0, 0 } });
        draw_list_generator.remove_tile(tile::Id { 1, { 0, 1 } });
        draw_list_generator.remove_tile(tile::Id { 1, { 1, 0 } });
        draw_list_generator.remove_tile(tile::Id { 1, { 1, 1 } });

        const auto list = draw_list_generator.generate_for(camera);
        REQUIRE(list.size() == 1);
        CHECK(list.contains(tile::Id { 0, { 0, 0 } }));
    }

}

TEST_CASE("nucleus/tile_scheduler/DrawListGenerator benchmark")
{
    std::vector<tile::Id> all_inner_nodes;
    quad_tree::onTheFlyTraverse(
        tile::Id{0, {0, 0}},
        [](const tile::Id &v) { return v.zoom_level < 7; },
        [&all_inner_nodes](const tile::Id &v) {
            all_inner_nodes.push_back(v);
            return v.children();
        });

    QFile file(":/map/height_data.atb");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    Q_UNUSED(open);
    const QByteArray data = file.readAll();
    const auto decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(TileHeights::deserialise(data));

    const auto camera_positions = std::vector{
        nucleus::camera::stored_positions::karwendel(),
        nucleus::camera::stored_positions::grossglockner(),
        nucleus::camera::stored_positions::oestl_hochgrubach_spitze(),
        nucleus::camera::stored_positions::schneeberg(),
        nucleus::camera::stored_positions::wien(),
        nucleus::camera::stored_positions::stephansdom(),
    };
    for (const auto &camera_position : camera_positions) {
        quad_tree::onTheFlyTraverse(tile::Id{0, {0, 0}},
                                    nucleus::tile_scheduler::utils::refineFunctor(camera_position,
                                                                                  decorator,
                                                                                  1),
                                    [&all_inner_nodes](const tile::Id &v) {
                                        all_inner_nodes.push_back(v);
                                        return v.children();
                                    });
    }

    nucleus::tile_scheduler::DrawListGenerator draw_list_generator;
    draw_list_generator.set_aabb_decorator(decorator);
    for (const auto &id : all_inner_nodes) {
        draw_list_generator.add_tile(id);
    }

    BENCHMARK("generate_for")
    {
        nucleus::tile_scheduler::DrawListGenerator::TileSet set;
        for (const auto &camera_position : camera_positions) {
            const auto list = draw_list_generator.generate_for(camera_position);
            set.reserve(set.size() + list.size());
            for (const auto &id : list) {
                set.insert(id);
            }
        }
        return set;
    };
}

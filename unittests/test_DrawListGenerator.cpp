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

#include "nucleus/camera/stored_positions.h"
#include "nucleus/tile_scheduler/DrawListGenerator.h"
#include "nucleus/tile_scheduler/utils.h"

#include "sherpa/TileHeights.h"
#include <catch2/catch.hpp>



TEST_CASE("nucleus/tile_scheduler/DrawListGenerator")
{
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    auto camera = camera::stored_positions::westl_hochgrubach_spitze();
    camera.set_viewport_size({ 1920, 1080 });

    DrawListGenerator draw_list_generator;
    draw_list_generator.set_aabb_decorator(tile_scheduler::AabbDecorator::make(std::move(h)));

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

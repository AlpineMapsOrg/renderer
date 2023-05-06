/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <catch2/catch_test_macros.hpp>

#include "nucleus/utils/terrain_mesh_index_generator.h"

TEST_CASE("nucleus/utils/terrain_mesh_index_generator")
{

    SECTION("surface quads 2x2")
    {
        const auto indices = nucleus::utils::terrain_mesh_index_generator::surface_quads<int>(2);
        REQUIRE(indices.size() == 4);
        CHECK(indices == std::vector({ 0, 2, 1, 3 }));
    }
    SECTION("surface quads 3x3")
    {
        const auto indices = nucleus::utils::terrain_mesh_index_generator::surface_quads<int>(3);
        const auto gt = std::vector({ 0, 3, 1, 4, 2, 5, 5, 8, 4, 7, 3, 6 });
        REQUIRE(indices.size() == gt.size());
        CHECK(indices == gt);
    }
    SECTION("surface quads 4x4")
    {
        const auto indices = nucleus::utils::terrain_mesh_index_generator::surface_quads<int>(4);
        const auto gt = std::vector({ 0, 4, 1, 5, 2, 6, 3, 7, 7, 11, 6, 10, 5, 9, 4, 8, 8, 12, 9, 13, 10, 14, 11, 15 });
        REQUIRE(indices.size() == gt.size());
        CHECK(indices == gt);
    }
    SECTION("surface quads 5x5")
    {
        const auto indices = nucleus::utils::terrain_mesh_index_generator::surface_quads<int>(5);
        const auto gt = std::vector({ 0, 5, 1, 6, 2, 7, 3, 8, 4, 9,
            9, 14, 8, 13, 7, 12, 6, 11, 5, 10,
            10, 15, 11, 16, 12, 17, 13, 18, 14, 19,
            19, 24, 18, 23, 17, 22, 16, 21, 15, 20 });
        REQUIRE(indices.size() == gt.size());
        CHECK(indices == gt);
    }
}

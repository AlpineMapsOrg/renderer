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
    using namespace nucleus::utils::terrain_mesh_index_generator;
    SECTION("surface quads 2x2")
    {
        const auto indices = surface_quads<int>(2);
        CHECK(indices == std::vector({ 0, 2, 1, 3 }));
    }
    SECTION("surface quads 3x3")
    {
        const auto indices = surface_quads<int>(3);
        const auto gt = std::vector({0, 3, 1, 4, 2, 5, 5, 3, 3, 6, 4, 7, 5, 8});
        CHECK(indices == gt);
    }
    SECTION("surface quads 4x4")
    {
        const auto indices = surface_quads<int>(4);
        const auto gt = std::vector({0, 4,  1, 5,  2,  6, 3, 7,  7, 4,  4,  8,  5,  9,
                                     6, 10, 7, 11, 11, 8, 8, 12, 9, 13, 10, 14, 11, 15});
        CHECK(indices == gt);
    }
    SECTION("surface quads with curtains 2x2")
    {
        const auto indices = surface_quads_with_curtains<int>(2);
        CHECK(indices == std::vector({0, 2, 1, 3, 3, 4, 1, 5, 0, 6, 2, 7, 3, 4}));
    }
    SECTION("surface quads with curtains 3x3")
    {
        const auto indices = surface_quads_with_curtains<int>(3);
        CHECK(indices == std::vector({0, 3,  1, 4,  2, 5,  5, 3,  3, 6,  4, 7,  5, 8,  8, 9,
                                      5, 10, 2, 11, 1, 12, 0, 13, 3, 14, 6, 15, 7, 16, 8, 9}));
    }
}

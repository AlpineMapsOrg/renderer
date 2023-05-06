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
#include <glm/glm.hpp>

#include "nucleus/Raster.h"
#include "unittests/test_helpers.h"

using nucleus::Raster;

TEST_CASE("nucleus/Raster")
{
    SECTION("empty and interface")
    {
        const Raster<uint16_t> raster;
        CHECK(raster.buffer().empty());
        CHECK(raster.width() == 0);
        CHECK(raster.height() == 0);
        CHECK(raster.begin() == raster.end());
        CHECK(raster.cbegin() == raster.cend());
        CHECK(raster.buffer_length() == 0);
    }

    SECTION("square default")
    {
        const Raster<uint16_t> raster(64);
        CHECK(raster.width() == 64);
        CHECK(raster.height() == 64);
    }
    SECTION("non-square default")
    {
        const Raster<uint16_t> raster(glm::uvec2(5, 10));
        CHECK(raster.width() == 5);
        CHECK(raster.height() == 10);
    }

    SECTION("move raster")
    {
        Raster<test_helpers::FailOnCopy> raster(1);
        const auto other = std::move(raster);
        CHECK(other.width() == 1);
        CHECK(other.height() == 1);
    }

    SECTION("move vector into raster")
    {
        std::vector<test_helpers::FailOnCopy> vector(1);
        const Raster<test_helpers::FailOnCopy> raster(std::move(vector), 1);
        CHECK(raster.width() == 1);
        CHECK(raster.height() == 1);
    }

    SECTION("write data")
    {
        Raster<int> raster(16);
        int i = 0;
        for (auto& v : raster) {
            v = i++;
        }

        i = 0;
        bool check = true;
        for (auto& v : raster) {
            check &= v == i++;
        }
        CHECK(check);
    }
}

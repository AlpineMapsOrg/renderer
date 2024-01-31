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
#include "test_helpers.h"

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
        const Raster<test_helpers::FailOnCopy> raster(1, std::move(vector));
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

    SECTION("pixel access")
    {
        Raster<int> raster({ 3, 4 });
        int i = 0;
        for (auto& v : raster) {
            v = i++;
        }
        CHECK(raster.pixel({ 0, 0 }) == 0);
        CHECK(raster.pixel({ 1, 0 }) == 1);
        CHECK(raster.pixel({ 2, 0 }) == 2);
        CHECK(raster.pixel({ 0, 1 }) == 3);
        CHECK(raster.pixel({ 1, 1 }) == 4);
        CHECK(raster.pixel({ 2, 1 }) == 5);
        CHECK(raster.pixel({ 2, 3 }) == i - 1);

        raster.pixel({ 2, 1 }) = 21;
        raster.pixel({ 2, 2 }) = 22;
        CHECK(raster.pixel({ 2, 1 }) == 21);
        CHECK(raster.pixel({ 2, 2 }) == 22);
    }

    SECTION("create filled")
    {
        Raster<int> raster({ 3, 4 }, 421);
        for (int p : raster) {
            CHECK(p == 421);
        }
    }

    SECTION("fill")
    {
        Raster<int> raster({ 3, 4 }, 421);
        raster.fill(657);
        for (int p : raster) {
            CHECK(p == 657);
        }
    }
}

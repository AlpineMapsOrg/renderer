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

#include "catch2_helpers.h"

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

    SECTION("resize 1")
    {
        Raster<int> a({ 4, 3 }, 1);
        const auto b = resize(a, { 2, 1 }, 2);
        CHECK(b.pixel({ 0, 0 }) == 1);
        CHECK(b.pixel({ 1, 0 }) == 1);
    }

    SECTION("resize 2")
    {
        Raster<int> a({ 2, 1 }, 1);
        const auto b = resize(a, { 4, 3 }, 2);
        CHECK(b.pixel({ 0, 0 }) == 1);
        CHECK(b.pixel({ 0, 1 }) == 2);
        CHECK(b.pixel({ 0, 2 }) == 2);

        CHECK(b.pixel({ 1, 0 }) == 1);
        CHECK(b.pixel({ 1, 1 }) == 2);
        CHECK(b.pixel({ 1, 2 }) == 2);

        for (auto i = 0u; i < 3; ++i) {
            CHECK(b.pixel({ 2, i }) == 2);
            CHECK(b.pixel({ 3, i }) == 2);
        }
    }

    SECTION("create mip maps")
    {
        // clang-format off
        Raster<glm::u8vec4> raster({ 4, 4 }, glm::u8vec4(0u, 0u, 0u, 0u));
        raster.pixel({ 0, 0 }) = glm::u8vec4(100u,   0u, 100u,   0u);
        raster.pixel({ 0, 1 }) = glm::u8vec4(100u,   0u, 100u, 150u);
        raster.pixel({ 1, 0 }) = glm::u8vec4(200u, 200u, 100u, 250u);
        raster.pixel({ 1, 1 }) = glm::u8vec4(0u,   200u, 100u,   0u);
        
        raster.pixel({ 0, 2 }) = glm::u8vec4(10u, 20u, 30u, 40u);
        raster.pixel({ 0, 3 }) = glm::u8vec4(10u, 20u, 30u, 40u);
        raster.pixel({ 1, 2 }) = glm::u8vec4(10u, 20u, 30u, 40u);
        raster.pixel({ 1, 3 }) = glm::u8vec4(10u, 20u, 30u, 40u);

        raster.pixel({ 2, 0 }) = glm::u8vec4(200u, 250u, 0u, 255u);
        raster.pixel({ 2, 1 }) = glm::u8vec4(200u, 250u, 0u, 255u);
        raster.pixel({ 3, 0 }) = glm::u8vec4(200u, 250u, 0u, 255u);
        raster.pixel({ 3, 1 }) = glm::u8vec4(200u, 250u, 0u, 255u);
        
        raster.pixel({ 2, 2 }) = glm::u8vec4(0u, 0u, 0u, 1u);
        raster.pixel({ 2, 3 }) = glm::u8vec4(3u, 0u, 0u, 1u);
        raster.pixel({ 3, 2 }) = glm::u8vec4(0u, 0u, 1u, 1u);
        raster.pixel({ 3, 3 }) = glm::u8vec4(0u, 1u, 2u, 1u);

        const auto mipmap = generate_mipmap(raster);
        REQUIRE(mipmap.size() == 3);
        CHECK(mipmap.at(0).size() == glm::uvec2(4, 4));
        CHECK(mipmap.at(1).size() == glm::uvec2(2, 2));
        CHECK(mipmap.at(2).size() == glm::uvec2(1, 1));

        for (auto i = 0u; i < raster.buffer_length(); ++i) {
            CHECK(mipmap.at(0).buffer().at(i) == raster.buffer().at(i));
        }
        CHECK(mipmap.at(1).pixel({ 0, 0 }) == glm::u8vec4(100u, 100u, 100u, 100u));
        CHECK(mipmap.at(1).pixel({ 0, 1 }) == glm::u8vec4(10u,   20u,  30u,  40u));
        CHECK(mipmap.at(1).pixel({ 1, 0 }) == glm::u8vec4(200u, 250u,   0u, 255u));
        CHECK(mipmap.at(1).pixel({ 1, 1 }) == glm::u8vec4(0u,     0u,   0u,   1u));
        
        CHECK(mipmap.at(2).pixel({ 0, 0 }) == glm::u8vec4(77u, 92u, 32u, 99u));
        // clang-format on
    }

    SECTION("create mip maps with uint16")
    {
        // clang-format off
        Raster<uint16_t> raster({ 2, 2 }, 0u);
        raster.pixel({ 0, 0 }) = 65535u;
        raster.pixel({ 0, 1 }) = 65535u;
        raster.pixel({ 1, 0 }) = 65535u;
        raster.pixel({ 1, 1 }) = 65535u;
        
        const auto mipmap = generate_mipmap(raster);
        REQUIRE(mipmap.size() == 2);
        CHECK(mipmap.at(0).size() == glm::uvec2(2, 2));
        CHECK(mipmap.at(1).size() == glm::uvec2(1, 1));
        
        CHECK(mipmap.at(0).pixel({ 0, 0 }) == 65535u);
        CHECK(mipmap.at(0).pixel({ 0, 1 }) == 65535u);
        CHECK(mipmap.at(0).pixel({ 1, 0 }) == 65535u);
        CHECK(mipmap.at(0).pixel({ 1, 1 }) == 65535u);
        
        CHECK(mipmap.at(1).pixel({ 0, 0 }) == 65535u);
        // clang-format on
    }

    SECTION("combine")
    {
        Raster<int> raster1({ 3, 4 }, 421);
        Raster<int> raster2({ 3, 2 }, 657);
        raster1.combine(raster2);

        CHECK(raster1.width() == 3);
        CHECK(raster1.height() == 6);

        CHECK(raster1.pixel({ 0, 0 }) == 421);
        CHECK(raster1.pixel({ 0, 3 }) == 421);
        CHECK(raster1.pixel({ 2, 3 }) == 421);

        CHECK(raster1.pixel({ 0, 4 }) == 657);
        CHECK(raster1.pixel({ 2, 4 }) == 657);
        CHECK(raster1.pixel({ 2, 5 }) == 657);
    }
}

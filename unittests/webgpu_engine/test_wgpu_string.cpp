/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Matthias Huerbe
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
#include <cstring>
#include <string>
#include <string_view>
#include <webgpu/base/wgpu_string.h>

using namespace webgpu;

static_assert(""_wsv.length == 0);
static_assert("hello"_wsv.length == 5);
static_assert("a\0b"_wsv.length == 3);

TEST_CASE("wsv literal - construct", "[wgpu_string]")
{
    auto str = "hello"_wsv;
    CHECK(str.length == 5);
    CHECK(std::memcmp(str.data, "hello", 5) == 0);
}

TEST_CASE("wsv literal - empty", "[wgpu_string]")
{
    CHECK(""_wsv.length == 0);
}

TEST_CASE("wsv literal - keeps embedded NUL", "[wgpu_string]")
{
    auto str = "a\0b"_wsv;
    CHECK(str.length == 3);
    CHECK(std::memcmp(str.data, "a\0b", 3) == 0);
}

TEST_CASE("wsv() - from string_view / string / const char*", "[wgpu_string]")
{
    std::string_view sv = "hello";
    CHECK(wsv(sv) == "hello"_wsv);

    std::string str = "world";
    CHECK(wsv(str) == "world"_wsv);

    CHECK(wsv("literal") == "literal"_wsv);

    CHECK(wsv(std::string_view("hello", 3)).length == 3);
    CHECK(wsv(std::string_view("hello", 3)) == "hel"_wsv);
}

TEST_CASE("wsv() - empty", "[wgpu_string]")
{
    CHECK(wsv(std::string_view {}).length == 0);
    CHECK(wsv("") == ""_wsv);
}

TEST_CASE("wsv_length - explicit length returned as-is", "[wgpu_string]")
{
    CHECK(wsv_length(WGPUStringView { "hello", 5 }) == 5);
    CHECK(wsv_length(WGPUStringView { "hello", 2 }) == 2);
    CHECK(wsv_length(WGPUStringView { "abc", 0 }) == 0);
}

TEST_CASE("wsv_length - WGPU_STRLEN sentinel uses strlen", "[wgpu_string]")
{
    CHECK(wsv_length(WGPUStringView { "hello", WGPU_STRLEN }) == 5);
    CHECK(wsv_length(WGPUStringView { "", WGPU_STRLEN }) == 0);
}

TEST_CASE("wsv_length - sentinel with null data is 0", "[wgpu_string]")
{
    CHECK(wsv_length(WGPUStringView { nullptr, WGPU_STRLEN }) == 0);
}

TEST_CASE("operator== - equal content", "[wgpu_string]")
{
    CHECK("hello"_wsv == WGPUStringView { "hello", 5 });
    CHECK("hello"_wsv == WGPUStringView { "hello", WGPU_STRLEN });
}

TEST_CASE("operator== - different content, same length", "[wgpu_string]")
{
    CHECK_FALSE("hello"_wsv == "world"_wsv);
    CHECK("hello"_wsv != "world"_wsv);
}

TEST_CASE("operator== - different length", "[wgpu_string]")
{
    CHECK_FALSE("hello"_wsv == "hell"_wsv);
    CHECK_FALSE("hello"_wsv == "hello!"_wsv);
}

TEST_CASE("operator== - empty views", "[wgpu_string]")
{
    CHECK(""_wsv == ""_wsv);
    CHECK(WGPUStringView { nullptr, WGPU_STRLEN } == ""_wsv);
    CHECK_FALSE(""_wsv == "x"_wsv);
}

TEST_CASE("operator== - NUL-safe, not strlen-based", "[wgpu_string]")
{
    CHECK("a\0b"_wsv == WGPUStringView { "a\0b", 3 });
    CHECK_FALSE("a\0b"_wsv == WGPUStringView { "a\0c", 3 });
}

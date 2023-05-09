/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "fmt/core.h"
#include "nucleus/tile_scheduler/Cache.h"

#include <unordered_set>

#include <QThread>
#include <catch2/catch_test_macros.hpp>

#include "sherpa/tile.h"


namespace {
struct TestTile {
    tile::Id id;
    std::string data;
};
}

TEST_CASE("nucleus/tile_scheduler/cache")
{
    SECTION("api")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.set_capacity(5);
        cache.insert({ TestTile { { 0, { 0, 0 } }, "root" } });
        CHECK(cache.contains({ 0, { 0, 0 } }));
        CHECK(cache.n_cached_objects() == 1);
        cache.visit([](const TestTile&) { return false; });
        std::vector<TestTile> removed_tiles = cache.purge();
    }

    SECTION("insert and visit")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.visit([](const TestTile &) {
            CHECK(false);
            return true;
        });
        CHECK(!cache.contains({ 0, { 0, 0 } }));
        CHECK(!cache.contains({ 6, { 4, 3 } }));
        CHECK(cache.n_cached_objects() == 0);
        cache.insert({
            TestTile { { 0, { 0, 0 } }, "green" },
            TestTile { { 6, { 4, 3 } }, "red" }, // yes, tiles going in do not need to be connected to the tree. and no, they won't be visited.
        });
        CHECK(cache.n_cached_objects() == 2);
        CHECK(cache.contains({ 0, { 0, 0 } }));
        CHECK(cache.contains({ 6, { 4, 3 } }));

        std::unordered_set<tile::Id, tile::Id::Hasher> visited;
        cache.visit([&visited](const TestTile& t) {
            CHECK(t.data == "green");
            visited.insert(t.id);
            return true;
        });
        CHECK(visited.size() == 1);

        cache.insert({
            TestTile { { 1, { 0, 0 } }, "green" },
            TestTile { { 1, { 1, 0 } }, "green" },
        });
        visited = {};
        cache.visit([&visited](const TestTile& t) {
            CHECK(t.data == "green");
            visited.insert(t.id);
            return true;
        });
        CHECK(visited.size() == 3);
        CHECK(visited.contains({ 0, { 0, 0 } }));
        CHECK(visited.contains({ 1, { 0, 0 } }));
        CHECK(visited.contains({ 1, { 1, 0 } }));

        cache.insert({
            TestTile { { 2, { 0, 0 } }, "green" },
            TestTile { { 3, { 0, 0 } }, "green" },
        });
        visited = {};
        cache.visit([&visited](const TestTile& t) {
            CHECK(t.data == "green");
            visited.insert(t.id);
            return true;
        });
        CHECK(visited.size() == 5);
        CHECK(visited.contains({ 0, { 0, 0 } }));
        CHECK(visited.contains({ 1, { 0, 0 } }));
        CHECK(visited.contains({ 1, { 1, 0 } }));
        CHECK(visited.contains({ 2, { 0, 0 } }));
        CHECK(visited.contains({ 3, { 0, 0 } }));
    }

    SECTION("visit can refuse to visit certain tiles")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.insert({
            TestTile { { 0, { 0, 0 } }, "green" },
            TestTile { { 1, { 0, 0 } }, "orange" },
            TestTile { { 1, { 0, 1 } }, "orange" },
            TestTile { { 1, { 1, 0 } }, "orange" },
            TestTile { { 1, { 1, 1 } }, "orange" },
            TestTile { { 2, { 0, 0 } }, "red" },
            TestTile { { 6, { 4, 3 } }, "red" }, // yes, tiles going in do not need to be connected to the tree. and no, they won't be visited.
        });

        std::unordered_set<tile::Id, tile::Id::Hasher> visited;
        cache.visit([&visited](const TestTile& t) {
            CHECK(t.data != "red");
            visited.insert(t.id);
            return t.data == "green";
        });
        CHECK(visited.size() == 5);
        CHECK(visited.contains({ 0, { 0, 0 } }));
        CHECK(visited.contains({ 1, { 0, 0 } }));
        CHECK(visited.contains({ 1, { 0, 1 } }));
        CHECK(visited.contains({ 1, { 1, 0 } }));
        CHECK(visited.contains({ 1, { 1, 1 } }));
    }

    SECTION("purge: all elements equal, large zoom levels first")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.insert({
            TestTile { { 0, { 0, 0 } }, "green" },
            TestTile { { 6, { 4, 3 } }, "red" },
            TestTile { { 1, { 0, 0 } }, "green" },
            TestTile { { 1, { 1, 0 } }, "green" },
            TestTile { { 2, { 0, 0 } }, "green" },
            TestTile { { 3, { 0, 0 } }, "green" },
        });
        cache.purge(); // default capacity is large enough for 6
        CHECK(cache.n_cached_objects() == 6);
        cache.set_capacity(3);
        const auto purged = cache.purge();
        REQUIRE(purged.size() == 3);
        CHECK(std::find_if(purged.cbegin(), purged.cend(), [](const auto& v) { return v.id == tile::Id { 2, { 0, 0 } };}) != purged.cend());
        CHECK(std::find_if(purged.cbegin(), purged.cend(), [](const auto& v) { return v.id == tile::Id { 6, { 4, 3 } };}) != purged.cend());
        CHECK(std::find_if(purged.cbegin(), purged.cend(), [](const auto& v) { return v.id == tile::Id { 3, { 0, 0 } };}) != purged.cend());
        CHECK(cache.n_cached_objects() == 3);
        CHECK(cache.contains({ 0, { 0, 0 } }));
        CHECK(cache.contains({ 1, { 0, 0 } }));
        CHECK(cache.contains({ 1, { 1, 0 } }));
    }

    SECTION("purge: elements coming in earlier are purged first")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.set_capacity(2);
        cache.insert({
            TestTile { { 0, { 0, 0 } }, "red" },
            TestTile { { 1, { 0, 1 } }, "red" },
        });
        QThread::msleep(2);
        cache.insert({
            TestTile { { 1, { 0, 0 } }, "green" },
            TestTile { { 1, { 1, 1 } }, "green" },
        });
        const auto purged = cache.purge();
        REQUIRE(purged.size() == 2);
        CHECK(purged[0].id == tile::Id { 0, { 0, 0 } }); // order does not matter
        CHECK(purged[1].id == tile::Id { 1, { 0, 1 } });
        CHECK(cache.n_cached_objects() == 2);
        CHECK(cache.contains({ 1, { 0, 0 } }));
        CHECK(cache.contains({ 1, { 1, 1 } }));
    }

    SECTION("purge: visit updates the time")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.set_capacity(2);
        cache.insert({
            TestTile { { 0, { 0, 0 } }, "older" },
        });
        QThread::msleep(2);
        cache.insert({
            TestTile { { 1, { 0, 0 } }, "newer" },
            TestTile { { 1, { 0, 1 } }, "newer" },
            TestTile { { 1, { 1, 0 } }, "newer" },
        });
        const std::unordered_set<tile::Id, tile::Id::Hasher> newer_tiles = {{ 1, { 0, 0 } }, { 1, { 0, 1 } }, { 1, { 1, 0 } }};
        QThread::msleep(2);
        cache.visit([](const TestTile &) { return true; });
        const auto purged = cache.purge();
        REQUIRE(purged.size() == 2);
        for (const auto& t : purged) {
            CHECK(!cache.contains(t.id));
            CHECK(newer_tiles.contains(t.id));
        }
        CHECK(purged[0].id != purged[1].id);
        CHECK(cache.n_cached_objects() == 2);
        CHECK(cache.contains({ 0, { 0, 0 } }));
    }

    SECTION("purge: visited elements are purged later than others")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.insert({
            TestTile { { 0, { 0, 0 } }, "green" },
            TestTile { { 2, { 0, 0 } }, "orange" },
        });
        QThread::msleep(2);
        cache.insert({
            TestTile { { 1, { 0, 0 } }, "green" },
            TestTile { { 1, { 1, 0 } }, "orange" },
            TestTile { { 1, { 1, 1 } }, "orange" },
        });
        QThread::msleep(2);
        cache.insert({
            TestTile { { 1, { 0, 1 } }, "orange" },
            TestTile { { 6, { 4, 3 } }, "red" }, // yes, tiles going in do not need to be connected to the tree. and no, they won't be visited.
        });
        CHECK(cache.n_cached_objects() == 7);

        QThread::msleep(2);
        std::unordered_set<tile::Id, tile::Id::Hasher> visited;
        cache.visit([&visited](const TestTile& t) {
            CHECK(t.data != "red");
            visited.insert(t.id);
            return t.data == "green";
        });
        cache.set_capacity(2);
        const auto purged = cache.purge();
        REQUIRE(purged.size() == 5);
        unsigned cnt_orange = 0;
        unsigned cnt_red = 0;
        for (const auto& t : purged) {
            cnt_orange += t.data == "orange";
            cnt_red += t.data == "red";
        }
        CHECK(cnt_orange == 4);
        CHECK(cnt_red == 1);
        CHECK(cache.n_cached_objects() == 2);
        CHECK(cache.contains({ 0, { 0, 0 } }));
        CHECK(cache.contains({ 1, { 0, 0 } }));
    }

    SECTION("insert: insert overwrites existing objects")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.insert({
            TestTile { { 0, { 0, 0 } }, "green" },
            TestTile { { 1, { 0, 1 } }, "green" },
        });
        cache.visit([](const TestTile& t) {
            CHECK(t.data == "green");
            return true;
        });
        cache.insert({
            TestTile { { 0, { 0, 0 } }, "red" },
            TestTile { { 1, { 0, 1 } }, "red" },
        });
        cache.visit([](const TestTile& t) {
            CHECK(t.data == "red");
            return true;
        });
    }
}

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

#include "nucleus/tile_scheduler/Cache.h"

#include <catch2/catch.hpp>

#include "sherpa/tile.h"

#include <QThread>
#include <unordered_set>

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
        cache.visit([](const TestTile& t) { CHECK(false); return true; });
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
        CHECK(purged.size() == 3);
        CHECK(purged[0].id == tile::Id { 2, { 0, 0 } }); // order does not matter
        CHECK(purged[1].id == tile::Id { 6, { 4, 3 } });
        CHECK(purged[2].id == tile::Id { 3, { 0, 0 } });
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
        CHECK(purged.size() == 2);
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
        QThread::msleep(2);
        cache.visit([](const TestTile& t) {
            return true;
        });
        const auto purged = cache.purge();
        CHECK(purged.size() == 2);
        CHECK(purged[0].id == tile::Id { 1, { 1, 0 } }); // order does not matter
        CHECK(purged[1].id == tile::Id { 1, { 0, 1 } });
        CHECK(cache.n_cached_objects() == 2);
        CHECK(cache.contains({ 0, { 0, 0 } }));
        CHECK(cache.contains({ 1, { 0, 0 } })); // might be any of the other newer elements
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
        CHECK(purged.size() == 5);
        CHECK(purged[0].data == "orange"); // order does not matter
        CHECK(purged[1].data == "orange");
        CHECK(purged[2].data == "orange");
        CHECK(purged[3].data == "orange");
        CHECK(purged[4].data == "red");
        CHECK(cache.n_cached_objects() == 2);
        CHECK(cache.contains({ 0, { 0, 0 } }));
        CHECK(cache.contains({ 1, { 0, 0 } }));
    }
}

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

#include <unordered_set>
#include <sstream>

#include <catch2/catch_test_macros.hpp>
#include <fmt/core.h>
#include <QStandardPaths>
#include <QThread>

#include "nucleus/tile_scheduler/Cache.h"
#include "radix/tile.h"


namespace {
struct TestTile {
    tile::Id id;
    std::string data;
};
struct DiskWriteTestTileInner {
    tile::Id id;
    std::shared_ptr<QByteArray> data;
};
struct DiskWriteTestTile {
    tile::Id id;
    int meta_data = 0;
    unsigned n_children = 0;
    std::array<DiskWriteTestTileInner, 4> tiles;
    static constexpr std::array<char, 25> version_information = {"DiskWriteTestTile"};
};
static_assert(nucleus::tile_scheduler::tile_types::SerialisableTile<DiskWriteTestTile>);
struct DiskWriteTestTile2 {
    tile::Id id;
    unsigned n_children = 0;
    std::array<DiskWriteTestTileInner, 4> tiles;
    static constexpr const std::array<char, 25> version_information = {"DiskWriteTestTile2"};
};
static_assert(nucleus::tile_scheduler::tile_types::SerialisableTile<DiskWriteTestTile2>);
}

TEST_CASE("nucleus/tile_scheduler/cache")
{
    SECTION("api")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.insert(TestTile { { 0, { 0, 0 } }, "root" });
        CHECK(cache.contains({ 0, { 0, 0 } }));
        CHECK(cache.n_cached_objects() == 1);
        cache.visit([](const TestTile&) { return false; });
        std::vector<TestTile> removed_tiles = cache.purge(5);
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
        cache.insert(TestTile { { 0, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 6, { 4, 3 } }, "red" }); // yes, tiles going in do not need to be connected to the tree. and no, they won't be visited
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

        cache.insert(TestTile { { 1, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 1, { 1, 0 } }, "green" });

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

        cache.insert(TestTile { { 2, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 3, { 0, 0 } }, "green" });

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
        cache.insert(TestTile { { 0, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 1, { 0, 0 } }, "orange" });
        cache.insert(TestTile { { 1, { 0, 1 } }, "orange" });
        cache.insert(TestTile { { 1, { 1, 0 } }, "orange" });
        cache.insert(TestTile { { 1, { 1, 1 } }, "orange" });
        cache.insert(TestTile { { 2, { 0, 0 } }, "red" });
        cache.insert(TestTile { { 6, { 4, 3 } }, "red" });

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
        cache.insert(TestTile { { 2, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 3, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 6, { 4, 3 } }, "red" });
        cache.insert(TestTile{{0, {0, 0}}, "green"});
        cache.insert(TestTile{{1, {0, 0}}, "green"});
        cache.insert(TestTile{{1, {1, 0}}, "green"});

        CHECK(cache.purge(6).size() == 0);
        CHECK(cache.n_cached_objects() == 6);
        const auto purged = cache.purge(3);
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

        cache.insert(TestTile { { 0, { 0, 0 } }, "red" });
        cache.insert(TestTile { { 1, { 0, 1 } }, "red" });
        QThread::msleep(2);

        cache.insert(TestTile { { 1, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 1, { 1, 1 } }, "green" });

        const auto purged = cache.purge(2);
        REQUIRE(purged.size() == 2);
        CHECK(std::find_if(purged.cbegin(), purged.cend(), [](const auto& t) { return t.id == tile::Id { 0, { 0, 0 } }; }) != purged.end());
        CHECK(std::find_if(purged.cbegin(), purged.cend(), [](const auto& t) { return t.id == tile::Id { 1, { 0, 1 } }; }) != purged.end());
        CHECK(cache.n_cached_objects() == 2);
        CHECK(cache.contains({ 1, { 0, 0 } }));
        CHECK(cache.contains({ 1, { 1, 1 } }));
    }

    SECTION("purge: visit updates the time")
    {
        nucleus::tile_scheduler::Cache<TestTile> cache;
        cache.insert(TestTile { { 0, { 0, 0 } }, "older" });

        QThread::msleep(2);
        cache.insert(TestTile { { 1, { 0, 0 } }, "newer" });
        cache.insert(TestTile { { 1, { 0, 1 } }, "newer" });
        cache.insert(TestTile { { 1, { 1, 0 } }, "newer" });

        const std::unordered_set<tile::Id, tile::Id::Hasher> newer_tiles = { { 1, { 0, 0 } }, { 1, { 0, 1 } }, { 1, { 1, 0 } } };
        QThread::msleep(2);
        cache.visit([](const TestTile &) { return true; });
        const auto purged = cache.purge(2);
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
        cache.insert(TestTile { { 0, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 2, { 0, 0 } }, "orange" });

        QThread::msleep(2);
        cache.insert(TestTile { { 1, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 1, { 1, 0 } }, "orange" });
        cache.insert(TestTile { { 1, { 1, 1 } }, "orange" });

        QThread::msleep(2);
        cache.insert(TestTile { { 1, { 0, 1 } }, "orange" });
        cache.insert(TestTile { { 6, { 4, 3 } }, "red" });
        CHECK(cache.n_cached_objects() == 7);

        QThread::msleep(2);

        std::unordered_set<tile::Id, tile::Id::Hasher> visited;
        cache.visit([&visited](const TestTile& t) {
            CHECK(t.data != "red");
            visited.insert(t.id);
            return t.data == "green";
        });
        const auto purged = cache.purge(2);
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
        cache.insert(TestTile { { 0, { 0, 0 } }, "green" });
        cache.insert(TestTile { { 1, { 0, 1 } }, "green" });

        cache.visit([](const TestTile& t) {
            CHECK(t.data == "green");
            return true;
        });

        cache.insert(TestTile { { 0, { 0, 0 } }, "red" });
        cache.insert(TestTile { { 1, { 0, 1 } }, "red" });

        cache.visit([](const TestTile& t) {
            CHECK(t.data == "red");
            return true;
        });
    }

    const auto create_test_tile = [](const tile::Id& id, int meta_data = 0) {
        auto t = DiskWriteTestTile {id, meta_data, 0, {}};

        for (const auto& child_id : id.children()) {
            std::stringstream ss;
            for (int i = 0; i < 1000; ++i)
                ss << child_id;
            t.tiles[t.n_children++] = {child_id, std::make_shared<QByteArray>(ss.str().c_str())};
        }
        return t;
    };
    const auto verify_tile = [](const auto& cache, const tile::Id& id, int meta_data = 0) {
        REQUIRE(cache.contains(id));
        const auto& tile = cache.peak_at(id);
        CHECK(tile.id == id);
        CHECK(tile.meta_data == meta_data);
        CHECK(tile.n_children == 4);
        const auto ref_children = id.children();
        for (unsigned i = 0; i < 4; ++i) {
            CHECK(tile.tiles[i].id == ref_children[i]);
            REQUIRE(tile.tiles[i].data);
            std::stringstream ss;
            for (int j = 0; j < 1000; ++j)
                ss << ref_children[i];
            QByteArray ref_ba(ss.str().c_str());
            CHECK(*(tile.tiles[i].data) == ref_ba);
        }
    };

    SECTION("write to disk and read back") {
        const auto path = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString()) / "test_tile_cache";
        std::filesystem::remove_all(path);
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            cache.insert(create_test_tile({ 0, { 0, 0 } }));
            cache.insert(create_test_tile({ 356, { 20, 564 } }));
            for (unsigned i = 1; i < 10; ++i) {
                cache.insert(create_test_tile({ i, { 0, 0 } }));
                cache.insert(create_test_tile({ i, { 1, 0 } }));
                cache.insert(create_test_tile({ i, { 1, 1 } }));
                cache.insert(create_test_tile({ i, { 0, 1 } }));
            }
            const auto r = cache.write_to_disk(path);
            std::string err;
            if (!r.has_value())
                err = r.error();
            CHECK(r.has_value());
        }
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;

            CHECK(cache.read_from_disk(path).has_value());
            verify_tile(cache, {0, {0, 0}});
            verify_tile(cache, {356, {20, 564}});
            for (unsigned i = 1; i < 10; ++i) {
                  verify_tile(cache, {i, {0, 0}});
                  verify_tile(cache, {i, {1, 0}});
                  verify_tile(cache, {i, {1, 1}});
                  verify_tile(cache, {i, {0, 1}});
            }
        }
        std::filesystem::remove_all(path);
    }

    SECTION("reading disk cache back fails on bad version") {
        const auto path = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString()) / "test_tile_cache";
        std::filesystem::remove_all(path);
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            cache.insert(create_test_tile({ 0, { 0, 0 } }));
            cache.insert(create_test_tile({ 356, { 20, 564 } }));
            CHECK(cache.write_to_disk(path).has_value());
        }
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile2> cache;
            CHECK(!cache.read_from_disk(path).has_value());
        }
        std::filesystem::remove_all(path);
    }

    SECTION("write to disk and read back itteratively") {
        const auto path = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString()) / "test_tile_cache";
        std::filesystem::remove_all(path);
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            cache.insert(create_test_tile({ 0, { 0, 0 } }));
            cache.insert(create_test_tile({ 1, { 0, 0 } }));
            CHECK(cache.write_to_disk(path).has_value());
        }
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            CHECK(cache.read_from_disk(path).has_value());
            CHECK(cache.n_cached_objects() == 2);
            verify_tile(cache, {0, {0, 0}});
            verify_tile(cache, {1, {0, 0}});
            cache.insert(create_test_tile({ 2, { 0, 0 } }));
            cache.insert(create_test_tile({ 3, { 0, 0 } }));
            CHECK(cache.write_to_disk(path).has_value());
        }
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            CHECK(cache.read_from_disk(path).has_value());
            CHECK(cache.n_cached_objects() == 4);
            verify_tile(cache, {0, {0, 0}});
            verify_tile(cache, {1, {0, 0}});
            verify_tile(cache, {2, {0, 0}});
            verify_tile(cache, {3, {0, 0}});
            cache.visit([](const auto&){return true;});
            cache.purge(3);
            CHECK(cache.n_cached_objects() == 3);
            verify_tile(cache, {0, {0, 0}});
            verify_tile(cache, {1, {0, 0}});
            verify_tile(cache, { 2, { 0, 0 } });
            CHECK(cache.write_to_disk(path).has_value());
        }
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            CHECK(cache.read_from_disk(path).has_value());
            CHECK(cache.n_cached_objects() == 3);
            verify_tile(cache, {0, {0, 0}});
            verify_tile(cache, {1, {0, 0}});
            verify_tile(cache, {2, {0, 0}});
        }
        std::filesystem::remove_all(path);
    }

    SECTION("disk cached tiles are updated when a tile is updated") {
        const auto path = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString()) / "test_tile_cache";
        std::filesystem::remove_all(path);
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            cache.insert(create_test_tile({ 0, { 0, 0 } }, 1));
            cache.insert(create_test_tile({ 1, { 0, 0 } }, 1));
            CHECK(cache.write_to_disk(path).has_value());

            QThread::msleep(2);
            cache.insert(create_test_tile({ 0, { 0, 0 } }, 2));
            cache.insert(create_test_tile({ 1, { 0, 0 } }, 2));
            CHECK(cache.write_to_disk(path).has_value());
        }
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            CHECK(cache.read_from_disk(path).has_value());
            CHECK(cache.n_cached_objects() == 2);
            verify_tile(cache, {0, {0, 0}}, 2);
            verify_tile(cache, {1, {0, 0}}, 2);
        }
        std::filesystem::remove_all(path);
    }


    SECTION("cache doesn't remember items that were in cache and on disk, but later deleted") {
        const auto path = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString()) / "test_tile_cache";
        std::filesystem::remove_all(path);
        {
            nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
            cache.insert(create_test_tile({ 3, { 0, 0 } }));
            cache.insert(create_test_tile({ 2, { 0, 0 } }));
            cache.insert(create_test_tile({ 1, { 0, 0 } }));
            cache.insert(create_test_tile({ 0, { 0, 0 } }));
            CHECK(cache.write_to_disk(path).has_value());
            cache.purge(2);
            CHECK(cache.n_cached_objects() == 2);
            verify_tile(cache, {0, {0, 0}});
            verify_tile(cache, { 1, { 0, 0 } });
            CHECK(cache.write_to_disk(path).has_value());
            {
                nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
                CHECK(cache.read_from_disk(path).has_value());
                CHECK(cache.n_cached_objects() == 2);
                verify_tile(cache, {0, {0, 0}});
                verify_tile(cache, {1, {0, 0}});
            }
            cache.insert(create_test_tile({ 2, { 0, 0 } }));
            cache.insert(create_test_tile({ 3, { 0, 0 } }));
            CHECK(cache.n_cached_objects() == 4);
            verify_tile(cache, {0, {0, 0}});
            verify_tile(cache, {1, {0, 0}});
            verify_tile(cache, {2, {0, 0}});
            verify_tile(cache, {3, {0, 0}});

            CHECK(cache.write_to_disk(path).has_value());
            {
                nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
                CHECK(cache.read_from_disk(path).has_value());
                CHECK(cache.n_cached_objects() == 4);
                verify_tile(cache, {0, {0, 0}});
                verify_tile(cache, {1, {0, 0}});
                verify_tile(cache, {2, {0, 0}});
                verify_tile(cache, {3, {0, 0}});
            }
            cache.visit([](const auto&){return true;});
            cache.purge(2);
            CHECK(cache.n_cached_objects() == 2);
            verify_tile(cache, {0, {0, 0}});
            verify_tile(cache, {1, {0, 0}});
            CHECK(cache.write_to_disk(path).has_value());
            {
                nucleus::tile_scheduler::Cache<DiskWriteTestTile> cache;
                CHECK(cache.read_from_disk(path).has_value());
                CHECK(cache.n_cached_objects() == 2);
                verify_tile(cache, {0, {0, 0}});
                verify_tile(cache, {1, {0, 0}});
            }
        }
        std::filesystem::remove_all(path);
    }
}

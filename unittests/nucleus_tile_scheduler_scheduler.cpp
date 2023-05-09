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

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <QSignalSpy>
#include <QThread>

#include "nucleus/camera/stored_positions.h"
#include "nucleus/tile_scheduler/Scheduler.h"
#include "nucleus/tile_scheduler/tile_types.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"
#include "sherpa/TileHeights.h"
#include "unittests/catch2_helpers.h"
#include "unittests/test_helpers.h"

namespace {

std::unique_ptr<nucleus::tile_scheduler::Scheduler> scheduler_with_disk_cache()
{
    static auto ortho_tile = nucleus::tile_scheduler::Scheduler::white_jpeg_tile(256);
    static auto height_tile = nucleus::tile_scheduler::Scheduler::black_png_tile(64);

    auto scheduler = std::make_unique<nucleus::tile_scheduler::Scheduler>(ortho_tile, height_tile);
    QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    scheduler->set_aabb_decorator(nucleus::tile_scheduler::utils::AabbDecorator::make(std::move(h)));
    scheduler->set_update_timeout(1);
    scheduler->set_enabled(true);
    spy.wait(2); // wait for quad requests triggered by set_enabled
    REQUIRE(spy.size() == 1);
    return scheduler;
}

std::unique_ptr<nucleus::tile_scheduler::Scheduler> default_scheduler()
{
    std::filesystem::remove_all(nucleus::tile_scheduler::Scheduler::disk_cache_path());
    return scheduler_with_disk_cache();
}
std::unique_ptr<nucleus::tile_scheduler::Scheduler> scheduler_with_aabb()
{
    std::filesystem::remove_all(nucleus::tile_scheduler::Scheduler::disk_cache_path());
    auto scheduler = std::make_unique<nucleus::tile_scheduler::Scheduler>();
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    scheduler->set_aabb_decorator(nucleus::tile_scheduler::utils::AabbDecorator::make(std::move(h)));
    return scheduler;
}

std::pair<QByteArray, QByteArray> example_tile_data()
{
    static const auto ortho_bytes = []() {
        auto ortho_file = QFile(QString("%1%2").arg(ALP_TEST_DATA_DIR, "test-tile_ortho.jpeg"));
        ortho_file.open(QFile::ReadOnly);
        const auto ortho_bytes = ortho_file.readAll();
        REQUIRE(!nucleus::utils::tile_conversion::toQImage(ortho_bytes).isNull());
        return ortho_bytes;
    }();

    static const auto height_bytes = []() {
        auto height_file = QFile(QString("%1%2").arg(ALP_TEST_DATA_DIR, "test-tile.png"));
        height_file.open(QFile::ReadOnly);
        const auto height_bytes = height_file.readAll();
        REQUIRE(!nucleus::utils::tile_conversion::toQImage(height_bytes).isNull());
        return height_bytes;
    }();

    return std::make_pair(ortho_bytes, height_bytes);
}

nucleus::tile_scheduler::tile_types::TileQuad example_tile_quad_for(const tile::Id& id, unsigned n_children = 4)
{
    const auto children = id.children();
    REQUIRE(n_children <= 4);
    nucleus::tile_scheduler::tile_types::TileQuad cpu_quad;
    cpu_quad.id = id;
    cpu_quad.n_tiles = n_children;
    const auto example_data = example_tile_data();
    for (unsigned i = 0; i < n_children; ++i) {
        cpu_quad.tiles[i].id = children[i];
        cpu_quad.tiles[i].ortho = std::make_shared<const QByteArray>(example_data.first);
        cpu_quad.tiles[i].height = std::make_shared<const QByteArray>(example_data.second);
    }
    return cpu_quad;
}

std::vector<nucleus::tile_scheduler::tile_types::TileQuad> example_quads_for_steffl_and_gg()
{
    static std::vector<nucleus::tile_scheduler::tile_types::TileQuad> retval = {
        example_tile_quad_for(tile::Id { 0, { 0, 0 } }),
        example_tile_quad_for(tile::Id { 1, { 1, 1 } }),
        example_tile_quad_for(tile::Id { 2, { 2, 2 } }),
        example_tile_quad_for(tile::Id { 3, { 4, 5 } }),
        example_tile_quad_for(tile::Id { 4, { 8, 10 } }),
        example_tile_quad_for(tile::Id { 5, { 17, 20 } }),
        example_tile_quad_for(tile::Id { 6, { 34, 41 } }),
        example_tile_quad_for(tile::Id { 7, { 69, 83 } }), // stephans dom
        example_tile_quad_for(tile::Id { 8, { 139, 167 } }),
        example_tile_quad_for(tile::Id { 9, { 279, 334 } }),
        example_tile_quad_for(tile::Id { 10, { 558, 668 } }),
        example_tile_quad_for(tile::Id { 10, { 558, 669 } }),
        example_tile_quad_for(tile::Id { 11, { 1117, 1337 } }),
        example_tile_quad_for(tile::Id { 11, { 1117, 1338 } }),
        example_tile_quad_for(tile::Id { 11, { 1116, 1337 } }),
        example_tile_quad_for(tile::Id { 11, { 1116, 1338 } }),
        example_tile_quad_for(tile::Id { 12, { 2234, 2675 } }),
        example_tile_quad_for(tile::Id { 7, { 68, 83 } }), // grossglockner
        example_tile_quad_for(tile::Id { 7, { 68, 82 } }),
        example_tile_quad_for(tile::Id { 8, { 136, 166 } }),
        example_tile_quad_for(tile::Id { 8, { 137, 166 } }),
        example_tile_quad_for(tile::Id { 8, { 136, 165 } }),
        example_tile_quad_for(tile::Id { 8, { 137, 165 } }),
        example_tile_quad_for(tile::Id { 9, { 273, 332 } }),
        example_tile_quad_for(tile::Id { 9, { 274, 332 } }),
        example_tile_quad_for(tile::Id { 9, { 273, 331 } }),
        example_tile_quad_for(tile::Id { 9, { 274, 331 } }),
        example_tile_quad_for(tile::Id { 10, { 547, 664 } }),
        example_tile_quad_for(tile::Id { 10, { 548, 664 } }),
        example_tile_quad_for(tile::Id { 11, { 1095, 1328 } }),
        example_tile_quad_for(tile::Id { 11, { 1096, 1328 } }),
        example_tile_quad_for(tile::Id { 12, { 2191, 2657 } }),
        example_tile_quad_for(tile::Id { 12, { 2192, 2657 } }),
        example_tile_quad_for(tile::Id { 12, { 2191, 2656 } }),
        example_tile_quad_for(tile::Id { 12, { 2192, 2656 } }),
        example_tile_quad_for(tile::Id { 13, { 4384, 5313 } }),
        example_tile_quad_for(tile::Id { 13, { 4385, 5313 } }),
        example_tile_quad_for(tile::Id { 13, { 4384, 5312 } }),
        example_tile_quad_for(tile::Id { 13, { 4385, 5312 } }),
    };
    return retval;
}
std::vector<nucleus::tile_scheduler::tile_types::TileQuad> example_quads_many()
{
    static std::vector<nucleus::tile_scheduler::tile_types::TileQuad> retval = []() {
        auto scheduler = default_scheduler();
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        auto camera = nucleus::camera::stored_positions::grossglockner();
        camera.set_viewport_size({ 3840, 2160 });
        scheduler->update_camera(camera);
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto quad_ids = spy.front().front().value<std::vector<tile::Id>>();

        std::vector<nucleus::tile_scheduler::tile_types::TileQuad> quads;
        quads.reserve(quad_ids.size());
        for (const auto& id : quad_ids) {
            quads.push_back(example_tile_quad_for(id));
        }

        return quads;
    }();
    return retval;
}

#ifdef __EMSCRIPTEN__
constexpr auto timing_multiplicator = 10;
#else
constexpr auto timing_multiplicator = 1;
#endif

}

TEST_CASE("nucleus/tile_scheduler/Scheduler")
{
    SECTION("enable / disable tile scheduler")
    {
        auto scheduler = scheduler_with_aabb();
        // disabled by default (enable by signal, in order to prevent a race with opengl)
        scheduler->set_update_timeout(1);

        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        CHECK(spy.empty());

        scheduler->set_enabled(true);
        spy.wait(2);
        CHECK(!spy.empty());
    }

    SECTION("update timeout & camera updates are collected")
    {
        auto scheduler = default_scheduler();
        scheduler->set_enabled(true);
        scheduler->set_update_timeout(5 * timing_multiplicator);

        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        scheduler->update_camera(nucleus::camera::stored_positions::oestl_hochgrubach_spitze());
        test_helpers::process_events_for(2 * timing_multiplicator);
        CHECK(spy.empty());

        test_helpers::process_events_for(5 * timing_multiplicator);
        CHECK(spy.size() == 1);
    }

    SECTION("timer is not restarted on camera updates && stops if nothing happens")
    {
        auto scheduler = default_scheduler();
        scheduler->set_update_timeout(6 * timing_multiplicator);
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        test_helpers::process_events_for(2 * timing_multiplicator);
        scheduler->update_camera(nucleus::camera::stored_positions::oestl_hochgrubach_spitze());
        test_helpers::process_events_for(2 * timing_multiplicator);
        CHECK(spy.empty());
        scheduler->update_camera(nucleus::camera::stored_positions::grossglockner());
        test_helpers::process_events_for(3 * timing_multiplicator);
        CHECK(spy.size() == 1);
        test_helpers::process_events_for(7 * timing_multiplicator);
        CHECK(spy.size() == 1);
    }

    SECTION("quads are being requested")
    {
        auto scheduler = default_scheduler();
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto quads = spy.constFirst().constFirst().value<std::vector<tile::Id>>();
        REQUIRE(quads.size() >= 5);
        // high level tiles that contain stephansdom
        // according to https://www.maptiler.com/google-maps-coordinates-tile-bounds-projection/#4/6.45/50.74
        // at the time of writing:
        // the corresponding tile list generation code is tested only by visual inspection (of the rendered scene).
        // seems to work. the following are sanity checks.
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 0, { 0, 0 } }) != quads.end());
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 1, { 1, 1 } }) != quads.end());
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 2, { 2, 2 } }) != quads.end());
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 3, { 4, 5 } }) != quads.end());
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 4, { 8, 10 } }) != quads.end());
    }

    SECTION("delivered quads are not requested again")
    {
        auto scheduler = default_scheduler();
        scheduler->receive_quads({
            example_tile_quad_for(tile::Id { 0, { 0, 0 } }),
            example_tile_quad_for(tile::Id { 1, { 1, 1 } }),
            example_tile_quad_for(tile::Id { 2, { 2, 2 } }),
        });
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::quads_requested);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto quads = spy.constFirst().constFirst().value<std::vector<tile::Id>>();
        REQUIRE(quads.size() >= 5);
        // high level tiles that contain stephansdom
        // according to https://www.maptiler.com/google-maps-coordinates-tile-bounds-projection/#4/6.45/50.74
        // at the time of writing:
        // the corresponding tile list generation code is tested only by visual inspection (of the rendered scene).
        // seems to work. the following are sanity checks.
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 0, { 0, 0 } }) == quads.end());
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 1, { 1, 1 } }) == quads.end());
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 2, { 2, 2 } }) == quads.end());
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 3, { 4, 5 } }) != quads.end());
        CHECK(std::find(quads.cbegin(), quads.cend(), tile::Id { 4, { 8, 10 } }) != quads.end());
    }

    SECTION("delivered quads are sent on to the gpu (with no repeat, only the ones in the tree)")
    {
        auto scheduler = default_scheduler();
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::gpu_quads_updated);
        scheduler->receive_quads({
            example_tile_quad_for(tile::Id { 0, { 0, 0 } }),
            example_tile_quad_for(tile::Id { 1, { 1, 1 } }),
            example_tile_quad_for(tile::Id { 2, { 2, 2 } }),
            example_tile_quad_for(tile::Id { 4, { 8, 10 } }),
        });
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto gpu_quads = spy.constFirst().constFirst().value<std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>>();
        REQUIRE(gpu_quads.size() == 3);
        CHECK(gpu_quads[0].id == tile::Id { 0, { 0, 0 } }); // order does not matter
        CHECK(gpu_quads[1].id == tile::Id { 1, { 1, 1 } });
        CHECK(gpu_quads[2].id == tile::Id { 2, { 2, 2 } });

        scheduler->receive_quads({
            example_tile_quad_for(tile::Id { 3, { 4, 5 } }),
            example_tile_quad_for(tile::Id { 5, { 17, 20 } }),
            example_tile_quad_for(tile::Id { 6, { 34, 41 } }),
            example_tile_quad_for(tile::Id { 7, { 69, 83 } }),
        });
        spy.wait(2);
        REQUIRE(spy.size() == 2);
        const auto new_gpu_quads = spy[1].front().value<std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>>();
        REQUIRE(new_gpu_quads.size() == 5);
        CHECK(new_gpu_quads[0].id == tile::Id { 3, { 4, 5 } }); // order does not matter
        CHECK(new_gpu_quads[1].id == tile::Id { 4, { 8, 10 } });
        CHECK(new_gpu_quads[2].id == tile::Id { 5, { 17, 20 } });
        CHECK(new_gpu_quads[3].id == tile::Id { 6, { 34, 41 } });
        CHECK(new_gpu_quads[4].id == tile::Id { 7, { 69, 83 } });
    }

    SECTION("tiles sent to the gpu are unpacked")
    {
        const auto compare_bounds = [](const tile::SrsAndHeightBounds& a, const tile::SrsAndHeightBounds& b) {
            for (int i = 0; i < 3; ++i) {
                CHECK(a.min[i] == Approx(b.min[i]));
                CHECK(a.max[i] == Approx(b.max[i]));
            }
        };

        auto scheduler = default_scheduler();
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::gpu_quads_updated);
        scheduler->receive_quads({
            example_tile_quad_for({ 0, { 0, 0 } }, 4),
        });
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto gpu_quads = spy.constFirst().constFirst().value<std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>>();
        REQUIRE(gpu_quads.size() == 1);
        CHECK(gpu_quads[0].id == tile::Id { 0, { 0, 0 } });
        CHECK(gpu_quads[0].tiles[0].id == tile::Id { 1, { 0, 0 } });
        CHECK(gpu_quads[0].tiles[1].id == tile::Id { 1, { 1, 0 } });
        CHECK(gpu_quads[0].tiles[2].id == tile::Id { 1, { 0, 1 } });
        CHECK(gpu_quads[0].tiles[3].id == tile::Id { 1, { 1, 1 } });
        const auto wsmax = nucleus::srs::tile_bounds({ 0, { 0, 0 } }).max.x; // world space max
        const auto wsmin = nucleus::srs::tile_bounds({ 0, { 0, 0 } }).min.x;

        compare_bounds(gpu_quads[0].tiles[0].bounds, tile::SrsAndHeightBounds { { wsmin, wsmin, 100 }, { 0, 0, 46367.813102 } });
        compare_bounds(gpu_quads[0].tiles[1].bounds, tile::SrsAndHeightBounds { { 0, wsmin, 100 }, { wsmax, 0, 46367.813102 } });
        compare_bounds(gpu_quads[0].tiles[2].bounds, tile::SrsAndHeightBounds { { wsmin, 0, 100 }, { 0, wsmax, 46367.813102 } });
        compare_bounds(gpu_quads[0].tiles[3].bounds, tile::SrsAndHeightBounds { { 0, 0, 100 }, { wsmax, wsmax, 46367.813102 } });

        for (auto i = 0; i < 4; ++i) {
            REQUIRE(gpu_quads[0].tiles[i].ortho);
            CHECK(gpu_quads[0].tiles[i].ortho->width() == 256);
            CHECK(gpu_quads[0].tiles[i].ortho->height() == 256);
            REQUIRE(gpu_quads[0].tiles[i].height);
            CHECK(gpu_quads[0].tiles[i].height->width() == 64);
            CHECK(gpu_quads[0].tiles[i].height->height() == 64);
        }
    }

    SECTION("incomplete tiles are replaced with default ones, when sending to gpu")
    {
        auto scheduler = default_scheduler();
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::gpu_quads_updated);
        auto quads = example_tile_quad_for({ 0, { 0, 0 } }, 3);
        quads.tiles[2].ortho.reset();
        quads.tiles[2].height.reset();

        scheduler->receive_quads({
            quads,
        });
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto gpu_quads = spy.constFirst().constFirst().value<std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>>();
        REQUIRE(gpu_quads.size() == 1);
        for (auto i = 0; i < 3; ++i) {
            REQUIRE(gpu_quads[0].tiles[i].ortho);
            CHECK(gpu_quads[0].tiles[i].ortho->width() == 256);
            CHECK(gpu_quads[0].tiles[i].ortho->height() == 256);
            REQUIRE(gpu_quads[0].tiles[i].height);
            CHECK(gpu_quads[0].tiles[i].height->width() == 64);
            CHECK(gpu_quads[0].tiles[i].height->height() == 64);
        }
    }

    SECTION("gpu quads are updated when serving from cache")
    {
        auto scheduler = default_scheduler();
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::gpu_quads_updated);
        scheduler->receive_quads(example_quads_for_steffl_and_gg());

        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        CHECK(spy.size() == 1);

        scheduler->update_camera(nucleus::camera::stored_positions::grossglockner());
        spy.wait(2);
        CHECK(spy.size() == 2);
    }

    SECTION("number of gpu quads doesn't exceede the limit")
    {
        auto scheduler = default_scheduler();
        scheduler->set_gpu_quad_limit(17);
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::gpu_quads_updated);
        scheduler->receive_quads(example_quads_for_steffl_and_gg());

        std::unordered_set<tile::Id, tile::Id::Hasher> cached_tiles;

        {
            scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
            spy.wait(2);
            REQUIRE(spy.size() == 1);
            const auto new_quads = spy[0][0].value<std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>>();
            const auto deleted_quads = spy[0][1].value<std::vector<tile::Id>>();
            CHECK(new_quads.size() == 17);
            CHECK(deleted_quads.empty());
            nucleus::tile_scheduler::Cache<nucleus::tile_scheduler::tile_types::GpuTileQuad> test_cache;
            test_cache.insert(new_quads);
            auto n_tiles = 0;
            test_cache.visit([&n_tiles](const auto&) {
                n_tiles++;
                return true;
            });
            CHECK(n_tiles == 17); // check that all are reachable
            for (const auto& tile : new_quads) {
                cached_tiles.insert(tile.id);
            }
        }

        {
            scheduler->update_camera(nucleus::camera::stored_positions::grossglockner());
            spy.wait(2);
            REQUIRE(spy.size() == 2);
            const auto new_quads = spy[1][0].value<std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>>();
            const auto deleted_quads = spy[1][1].value<std::vector<tile::Id>>();
            CHECK(new_quads.size() == deleted_quads.size());
            CHECK(!new_quads.empty());

            { // check that all are reachable
                for (const auto& tile : new_quads) {
                    cached_tiles.insert(tile.id);
                }
                for (const auto& id : deleted_quads) {
                    cached_tiles.erase(id);
                }
                CHECK(cached_tiles.size() == 17);
                nucleus::tile_scheduler::Cache<nucleus::tile_scheduler::tile_types::GpuCacheInfo> test_cache;
                for (const auto& id : cached_tiles) {
                    test_cache.insert({ { id } });
                }
                auto n_tiles = 0;
                test_cache.visit([&n_tiles](const auto&) {
                    n_tiles++;
                    return true;
                });
                CHECK(n_tiles == 17);
            }
            { // check for double entries
                std::unordered_set<tile::Id, tile::Id::Hasher> deleted_quads_set(deleted_quads.cbegin(), deleted_quads.cend());
                for (const auto& new_quad : new_quads) {
                    CHECK(!deleted_quads_set.contains(new_quad.id));
                }
            }
        }
    }

    SECTION("gpu tiles are optimised for the current camera position")
    {
        auto scheduler = default_scheduler();
        scheduler->set_gpu_quad_limit(17);
        QSignalSpy spy(scheduler.get(), &nucleus::tile_scheduler::Scheduler::gpu_quads_updated);
        scheduler->receive_quads(example_quads_for_steffl_and_gg());

        std::unordered_set<tile::Id, tile::Id::Hasher> cached_tiles;
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        spy.wait(2);
        REQUIRE(spy.size() == 1);
        const auto new_quads = spy[0][0].value<std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>>();
        for (const auto& tile : new_quads) {
            cached_tiles.insert(tile.id);
        }
        CHECK(cached_tiles.contains({ 11, { 1117, 1337 } }));
        CHECK(cached_tiles.contains({ 11, { 1117, 1338 } }));
        CHECK(cached_tiles.contains({ 11, { 1116, 1337 } }));
        CHECK(cached_tiles.contains({ 11, { 1116, 1338 } }));
        CHECK(cached_tiles.contains({ 12, { 2234, 2675 } }));
    }

    SECTION("ram tiles are purged")
    {
        auto scheduler = default_scheduler();
        scheduler->set_ram_quad_limit(17);
        scheduler->set_purge_timeout(1);
        scheduler->receive_quads(example_quads_for_steffl_and_gg());
        test_helpers::process_events_for(2);
        CHECK(scheduler->ram_cache().n_cached_objects() == 17);
    }

    SECTION("purging tiles based on camera")
    {
        auto scheduler = default_scheduler();
        scheduler->set_ram_quad_limit(17);
        scheduler->set_purge_timeout(1);
        scheduler->update_camera(nucleus::camera::stored_positions::stephansdom());
        scheduler->receive_quads(example_quads_for_steffl_and_gg());
        test_helpers::process_events_for(2);
        CHECK(scheduler->ram_cache().n_cached_objects() == 17);
        CHECK(scheduler->ram_cache().contains({ 11, { 1117, 1337 } }));
        CHECK(scheduler->ram_cache().contains({ 11, { 1117, 1338 } }));
        CHECK(scheduler->ram_cache().contains({ 11, { 1116, 1337 } }));
        CHECK(scheduler->ram_cache().contains({ 11, { 1116, 1338 } }));
        CHECK(scheduler->ram_cache().contains({ 12, { 2234, 2675 } }));
    }

    SECTION("purging ram tiles with tolerance")
    {
        auto scheduler = default_scheduler();
        // example_quads_for_steffl_and_gg().size() == 39
        const unsigned limit = 38;
        assert(example_quads_for_steffl_and_gg().size() > limit);
        scheduler->set_ram_quad_limit(limit);
        scheduler->set_purge_timeout(1);
        scheduler->receive_quads(example_quads_for_steffl_and_gg());
        test_helpers::process_events_for(2);
        CHECK(scheduler->ram_cache().n_cached_objects() == example_quads_for_steffl_and_gg().size());
        scheduler->receive_quads({
            example_tile_quad_for(tile::Id { 10, { 0, 0 } }),
            example_tile_quad_for(tile::Id { 11, { 1, 1 } }),
            example_tile_quad_for(tile::Id { 12, { 2, 2 } }),
        });
        test_helpers::process_events_for(2);
        CHECK(scheduler->ram_cache().n_cached_objects() == limit);
    }

    SECTION("purging happens with a delay (collects purge events) and the timer is not restarted on tile delivery")
    {
        auto scheduler = default_scheduler();
        scheduler->set_purge_timeout(9 * timing_multiplicator);
        scheduler->set_update_timeout(20 * timing_multiplicator); // sending quads to gpu takes long and makes tight timing impossible in debug mode
        scheduler->set_ram_quad_limit(2);
        scheduler->receive_quads({
            example_tile_quad_for(tile::Id { 0, { 0, 0 } }),
            example_tile_quad_for(tile::Id { 1, { 1, 1 } }),
            example_tile_quad_for(tile::Id { 2, { 2, 2 } }),
        });
        CHECK(scheduler->ram_cache().n_cached_objects() == 3);
        test_helpers::process_events_for(3 * timing_multiplicator);
        CHECK(scheduler->ram_cache().n_cached_objects() == 3);
        scheduler->receive_quads({
            example_tile_quad_for(tile::Id { 1, { 0, 0 } }),
            example_tile_quad_for(tile::Id { 1, { 1, 0 } }),
            example_tile_quad_for(tile::Id { 2, { 2, 1 } }),
        });

        CHECK(scheduler->ram_cache().n_cached_objects() == 6);
        test_helpers::process_events_for(3 * timing_multiplicator);
        CHECK(scheduler->ram_cache().n_cached_objects() == 6);

        scheduler->receive_quads({
            example_tile_quad_for(tile::Id { 1, { 0, 1 } }),
            example_tile_quad_for(tile::Id { 2, { 1, 1 } }),
            example_tile_quad_for(tile::Id { 2, { 1, 2 } }),
        });
        CHECK(scheduler->ram_cache().n_cached_objects() == 9);
        test_helpers::process_events_for(4 * timing_multiplicator);
        CHECK(scheduler->ram_cache().n_cached_objects() == 2);
    }

    SECTION("persisting data works")
    {
        {
            auto scheduler = default_scheduler();
            scheduler->receive_quads({
                example_tile_quad_for(tile::Id { 0, { 0, 0 } }),
                example_tile_quad_for(tile::Id { 1, { 1, 1 } }),
                example_tile_quad_for(tile::Id { 2, { 2, 2 } }),
            });
            scheduler->persist_tiles();
        }
        auto scheduler = scheduler_with_disk_cache();
        CHECK(scheduler->ram_cache().n_cached_objects() == 3);

        const auto ids = std::vector { tile::Id { 0, { 0, 0 } }, tile::Id { 1, { 1, 1 } }, tile::Id { 2, { 2, 2 } } };
        for (const auto& id : ids) {
            const auto example_quad = example_tile_quad_for(id);
            REQUIRE(scheduler->ram_cache().contains(id));
            REQUIRE(scheduler->ram_cache().peak_at(id).n_tiles == example_quad.n_tiles);
            REQUIRE(scheduler->ram_cache().peak_at(id).id == id);
            REQUIRE(id == example_quad.id);
            for (unsigned i = 0; i < 4; ++i) {
                CHECK(*scheduler->ram_cache().peak_at(id).tiles[i].height == *example_quad.tiles[i].height);
                CHECK(*scheduler->ram_cache().peak_at(id).tiles[i].ortho == *example_quad.tiles[i].ortho);
            }
        }

        CHECK(scheduler->ram_cache().peak_at(tile::Id { 0, { 0, 0 } }).tiles[0].id == tile::Id { 1, { 1, 1 } }); // order does not matter!
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 0, { 0, 0 } }).tiles[1].id == tile::Id { 1, { 0, 1 } });
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 0, { 0, 0 } }).tiles[2].id == tile::Id { 1, { 1, 0 } });
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 0, { 0, 0 } }).tiles[3].id == tile::Id { 1, { 0, 0 } });

        CHECK(scheduler->ram_cache().peak_at(tile::Id { 1, { 1, 1 } }).tiles[0].id == tile::Id { 2, { 3, 3 } });
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 1, { 1, 1 } }).tiles[1].id == tile::Id { 2, { 2, 3 } });
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 1, { 1, 1 } }).tiles[2].id == tile::Id { 2, { 3, 2 } });
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 1, { 1, 1 } }).tiles[3].id == tile::Id { 2, { 2, 2 } });

        CHECK(scheduler->ram_cache().peak_at(tile::Id { 2, { 2, 2 } }).tiles[0].id == tile::Id { 3, { 5, 5 } });
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 2, { 2, 2 } }).tiles[1].id == tile::Id { 3, { 4, 5 } });
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 2, { 2, 2 } }).tiles[2].id == tile::Id { 3, { 4, 4 } });
        CHECK(scheduler->ram_cache().peak_at(tile::Id { 2, { 2, 2 } }).tiles[3].id == tile::Id { 3, { 5, 4 } });
    }
}

TEST_CASE("nucleus/tile_scheduler/Scheduler benchmarks")
{
    auto camera = nucleus::camera::stored_positions::grossglockner();
    camera.set_viewport_size({ 3840, 2160 });

    BENCHMARK("construct")
    {
        return default_scheduler();
    };

    BENCHMARK("request quads")
    {
        auto scheduler = default_scheduler();
        scheduler->update_camera(camera);
        scheduler->send_quad_requests();
    };

    BENCHMARK("receive " + std::to_string(example_quads_many().size()) + " quads")
    {
        auto scheduler = default_scheduler();
        scheduler->receive_quads(example_quads_many());
    };

    BENCHMARK("receive " + std::to_string(example_quads_for_steffl_and_gg().size()) + " quads + update_gpu_quads")
    {
        auto scheduler = default_scheduler();
        scheduler->update_camera(camera);
        scheduler->receive_quads(example_quads_for_steffl_and_gg());
        // unpacking byte arrays takes long, hence only the smaller dataset
        scheduler->update_gpu_quads();
    };

    BENCHMARK("receive " + std::to_string(example_quads_many().size()) + " quads + purge_ram_cache")
    {
        auto scheduler = default_scheduler();
        scheduler->update_camera(camera);
        scheduler->receive_quads(example_quads_for_steffl_and_gg());
        scheduler->purge_ram_cache();
    };
}

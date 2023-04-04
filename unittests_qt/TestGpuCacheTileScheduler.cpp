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

#include "nucleus/tile_scheduler/GpuCacheTileScheduler.h"
#include "nucleus/tile_scheduler/utils.h"
#include "qtestcase.h"
#include "unittests_qt/TileScheduler.h"

#include <unordered_set>

#include <QSignalSpy>
#include <QTest>
#include <glm/glm.hpp>

#include "nucleus/camera/Definition.h"
#include "nucleus/camera/stored_positions.h"
#include "sherpa/TileHeights.h"

using nucleus::tile_scheduler::GpuCacheTileScheduler;

class TestGpuCacheTileScheduler : public TestTileScheduler {
    Q_OBJECT
private:
    std::unique_ptr<TileScheduler> makeScheduler() const override
    {
        auto sch = std::make_unique<GpuCacheTileScheduler>();
        TileHeights h;
        h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
        sch->set_aabb_decorator(nucleus::tile_scheduler::AabbDecorator::make(std::move(h)));
        sch->set_max_n_simultaneous_requests(5000);
        sch->set_update_timeout(1);
        sch->set_enabled(true);
        return sch;
    }

private slots:
    //  void initTestCase() {}    // implementing these functions will override TestTileScheduler and break the tests.
    //  void init() {}            // so call the TestTileScheduler::init and initTestCase somehow, then it should be good again.
    void init()
    {
        TestTileScheduler::init();
        dynamic_cast<GpuCacheTileScheduler*>(m_scheduler.get())->set_gpu_cache_size(0);
    }
    void loadCandidates()
    {
        const auto xy_world_space = srs::lat_long_to_world({ 10, 10 });
        const auto camera_definition = nucleus::camera::Definition({ xy_world_space.x, xy_world_space.y, 500 }, { xy_world_space.x + 10'000, xy_world_space.y, 500 });
        m_scheduler->update_camera(camera_definition);
        const auto tile_list = m_scheduler->load_candidates();
        //        for (const auto& candidate : m_scheduler->load_candidates()) {
        //            qDebug("m_scheduler->load_candidates(zoom: %u, x: %u, y: %u", candidate.zoom_level, candidate.coords.x, candidate.coords.y);
        //        }
        QVERIFY(!tile_list.empty());
    }

    void no_crash_with_0_cache_size()
    {
        dynamic_cast<GpuCacheTileScheduler*>(m_scheduler.get())->set_gpu_cache_size(0);
        QVERIFY(m_scheduler->gpu_tiles().empty());
        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);
        m_scheduler->update_camera(test_cam);
        QTest::qWait(10);
        // tiles are on the gpu
        const auto gpu_tiles = m_scheduler->gpu_tiles();

        QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_expired);
        nucleus::camera::Definition replacement_cam = nucleus::camera::stored_positions::westl_hochgrubach_spitze();
        replacement_cam.set_viewport_size({ 2560, 1440 });
        m_scheduler->update_camera(replacement_cam);
    }

    void expiresTiles()
    {
        m_scheduler->set_gpu_cache_size(400);
        m_scheduler->set_main_cache_size(500);
        QVERIFY(m_scheduler->gpu_tiles().empty());
        QVERIFY(m_scheduler->main_cache_book().size() == 0);
        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);
        m_scheduler->update_camera(test_cam);
        QTest::qWait(50); // several load cycles until all tiles are loaded
        const auto n_required_tiles1 = m_scheduler->load_candidates().size();

        {
            QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_expired);
            const auto gpu_tiles = m_scheduler->gpu_tiles();

            nucleus::camera::Definition replacement_cam = nucleus::camera::stored_positions::westl_hochgrubach_spitze();
            replacement_cam.set_viewport_size({ 1920 / 2, 1080 / 2 });
            m_scheduler->update_camera(replacement_cam);
            spy.wait(20);
            for (const auto& tileExpireSignal : spy) {
                const tile::Id tile = tileExpireSignal.at(0).value<tile::Id>();
                QVERIFY(gpu_tiles.contains(tile));
            }
        }
        const auto n_required_tiles2 = m_scheduler->load_candidates().size();
        QCOMPARE(m_scheduler->gpu_tiles().size(), 400);
        QCOMPARE(m_scheduler->main_cache_book().size(), unsigned(500 * 0.9));
        QCOMPARE(m_scheduler->main_cache_book().size(), m_scheduler->number_of_waiting_ortho_tiles());
        QCOMPARE(m_scheduler->main_cache_book().size(), m_scheduler->number_of_waiting_height_tiles());
        {
            QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_expired);
            const auto gpu_tiles = m_scheduler->gpu_tiles();

            nucleus::camera::Definition replacement_cam = nucleus::camera::stored_positions::grossglockner();
            replacement_cam.set_viewport_size({ 1920 / 2, 1080 / 2 });
            m_scheduler->update_camera(replacement_cam);
            spy.wait(20);
            for (const auto& tileExpireSignal : spy) {
                const tile::Id tile = tileExpireSignal.at(0).value<tile::Id>();
                QVERIFY(gpu_tiles.contains(tile));
            }
        }
        const auto n_required_tiles3 = m_scheduler->load_candidates().size();
        const auto n_orhpans_3 = m_scheduler->orphan_cache_book().size();
        for (const auto& candidate : m_scheduler->load_candidates()) {
            qDebug("m_scheduler->load_candidates(zoom: %u, x: %u, y: %u", candidate.zoom_level, candidate.coords.x, candidate.coords.y);
        }
        QCOMPARE(m_scheduler->gpu_tiles().size(), 400);
        QCOMPARE(m_scheduler->main_cache_book().size(), unsigned(500 * 0.9));
        QCOMPARE(m_scheduler->main_cache_book().size(), m_scheduler->number_of_waiting_height_tiles());
        QCOMPARE(m_scheduler->main_cache_book().size(), m_scheduler->number_of_waiting_ortho_tiles());
    }

    void expiresTilesThatWereReadFromDiskCache()
    {
        m_scheduler->set_gpu_cache_size(400);
        m_scheduler->set_main_cache_size(500);
        QVERIFY(m_scheduler->gpu_tiles().empty());
        QVERIFY(m_scheduler->main_cache_book().size() == 1);
        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);

        {
            m_scheduler->update_camera(test_cam);
            QTest::qWait(50); // several load cycles until all tiles are loaded
        }
        {
            nucleus::camera::Definition replacement_cam = nucleus::camera::stored_positions::westl_hochgrubach_spitze();
            replacement_cam.set_viewport_size({ 2560, 1440 });
            m_scheduler->update_camera(replacement_cam);
            QTest::qWait(50); // several load cycles until all tiles are loaded
        }
        {
            nucleus::camera::Definition replacement_cam = nucleus::camera::stored_positions::grossglockner();
            replacement_cam.set_viewport_size({ 2560, 1440 });
            m_scheduler->update_camera(replacement_cam);
            QTest::qWait(50); // several load cycles until all tiles are loaded
        }
        m_scheduler = makeScheduler();
        m_scheduler->set_gpu_cache_size(400);
        m_scheduler->set_main_cache_size(500);
        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);
        {
            QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_expired);
            const auto gpu_tiles = m_scheduler->gpu_tiles();

            nucleus::camera::Definition replacement_cam = nucleus::camera::stored_positions::westl_hochgrubach_spitze();
            replacement_cam.set_viewport_size({ 2560, 1440 });
            m_scheduler->update_camera(replacement_cam);
            spy.wait(20);
            for (const auto& tileExpireSignal : spy) {
                const tile::Id tile = tileExpireSignal.at(0).value<tile::Id>();
                QVERIFY(gpu_tiles.contains(tile));
            }
        }
        QCOMPARE(m_scheduler->gpu_tiles().size(), 400);
        QCOMPARE(m_scheduler->main_cache_book().size(), unsigned(500 * 0.9));
        QCOMPARE(m_scheduler->main_cache_book().size(), m_scheduler->number_of_waiting_ortho_tiles());
        QCOMPARE(m_scheduler->main_cache_book().size(), m_scheduler->number_of_waiting_height_tiles());
        {
            QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_expired);
            const auto gpu_tiles = m_scheduler->gpu_tiles();

            nucleus::camera::Definition replacement_cam = nucleus::camera::stored_positions::grossglockner();
            replacement_cam.set_viewport_size({ 2560, 1440 });
            m_scheduler->update_camera(replacement_cam);
            spy.wait(20);
            for (const auto& tileExpireSignal : spy) {
                const tile::Id tile = tileExpireSignal.at(0).value<tile::Id>();
                QVERIFY(gpu_tiles.contains(tile));
            }
        }
        QCOMPARE(m_scheduler->gpu_tiles().size(), 400);
        QCOMPARE(m_scheduler->main_cache_book().size(), unsigned(500 * 0.9));
        QCOMPARE(m_scheduler->main_cache_book().size(), m_scheduler->number_of_waiting_ortho_tiles());
        QCOMPARE(m_scheduler->main_cache_book().size(), m_scheduler->number_of_waiting_height_tiles());
    }
};

QTEST_MAIN(TestGpuCacheTileScheduler)
#include "TestGpuCacheTileScheduler.moc"

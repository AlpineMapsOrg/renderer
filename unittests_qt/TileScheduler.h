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

#include "nucleus/TileScheduler.h"

#include <unordered_set>

#include <QSignalSpy>
#include <QTest>
#include <glm/glm.hpp>

#include "nucleus/camera/Definition.h"
#include "nucleus/Tile.h"
#include "nucleus/camera/stored_positions.h"
#include "nucleus/srs.h"

class TestTileScheduler : public QObject {
    Q_OBJECT
protected:
    QByteArray m_ortho_bytes;
    QByteArray m_height_bytes;
    std::unique_ptr<TileScheduler> m_scheduler;
    camera::Definition test_cam = camera::stored_positions::stephansdom();
    std::unordered_set<tile::Id, tile::Id::Hasher> m_given_tiles;
    std::unordered_set<tile::Id, tile::Id::Hasher> m_unavailable_tiles;

    virtual std::unique_ptr<TileScheduler> makeScheduler() const = 0;

public slots:
    void giveTiles(const tile::Id& tile_id)
    {
        if (m_unavailable_tiles.contains(tile_id)) {
            emit tileUnavailable(tile_id);
            return;
        }
        m_given_tiles.insert(tile_id);
        emit orthoTileReady(tile_id, std::make_shared<QByteArray>(m_ortho_bytes));
        emit heightTileReady(tile_id, std::make_shared<QByteArray>(m_height_bytes));
    }

signals:
    void orthoTileReady(tile::Id tile_id, std::shared_ptr<QByteArray> data);
    void heightTileReady(tile::Id tile_id, std::shared_ptr<QByteArray> data);
    void tileUnavailable(tile::Id tile_id);

protected slots:
    void initTestCase()
    {
        auto ortho_file = QFile(QString("%1%2").arg(ALP_TEST_DATA_DIR, "test-tile_ortho.jpeg"));
        ortho_file.open(QFile::ReadOnly);
        m_ortho_bytes = ortho_file.readAll();
        QVERIFY(m_ortho_bytes.size() > 10);

        auto height_file = QFile(QString("%1%2").arg(ALP_TEST_DATA_DIR, "test-tile.png"));
        height_file.open(QFile::ReadOnly);
        m_height_bytes = height_file.readAll();
        QVERIFY(m_height_bytes.size() > 10);

        test_cam.set_viewport_size({ 2560, 1440 });
    }

    void init()
    {
        m_scheduler = makeScheduler();
        m_given_tiles.clear();
        m_unavailable_tiles.clear();
    }

    void emitsTileRequestsWhenUpdatingCamera()
    {
        QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_requested);
        QVERIFY(m_scheduler->number_of_tiles_in_transit() == 0);
        m_scheduler->update_camera(test_cam);
        spy.wait(5);

        QVERIFY(spy.size() >= 10);
        QVERIFY(m_scheduler->number_of_tiles_in_transit() == size_t(spy.size()));
        auto n_tiles_containing_camera_position = 0;
        auto n_tiles_containing_camera_view_dir = 0;
        for (const QList<QVariant>& signal : spy) { // yes, QSignalSpy is a QList<QList<QVariant>>, where the inner QList contains the signal arguments
            const auto tile_id = signal.at(0).value<tile::Id>();
            QVERIFY(tile_id.zoom_level < 30);
            const auto tile_bounds = srs::tile_bounds(tile_id);
            if (tile_bounds.contains(glm::dvec2(test_cam.position())))
                n_tiles_containing_camera_position++;
            if (tile_bounds.contains({ 1822577.0, 6141664.0 }))
                n_tiles_containing_camera_view_dir++;
        }
        QVERIFY(n_tiles_containing_camera_view_dir >= 1);
        QVERIFY(n_tiles_containing_camera_position >= 1);
    }

    void tileRequestsSentOnlyOnce()
    {
        {
            QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_requested);
            m_scheduler->update_camera(test_cam);
            spy.wait(10); // ignore first batch of requests
        }
        {
            QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_requested);
            m_scheduler->update_camera(test_cam);
            spy.wait(10);
            QVERIFY(spy.count() == 0); // the scheduler should know, that it already requested these tiles
        }
    }

    void emitsReceivedTiles()
    {
        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);
        QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_ready);
        m_scheduler->update_camera(test_cam);
        spy.wait(10);
        QVERIFY(m_given_tiles.size() >= 10);
        QVERIFY(size_t(spy.size()) == m_given_tiles.size());
        for (const QList<QVariant>& signal : spy) { // yes, QSignalSpy is a QList<QList<QVariant>>, where the inner QList contains the signal arguments
            const std::shared_ptr<Tile> tile = signal.at(0).value<std::shared_ptr<Tile>>();
            QVERIFY(tile);
            QVERIFY(m_given_tiles.contains(tile->id));
            QVERIFY(tile->height_map.width() == 256);
            QVERIFY(tile->height_map.height() == 256);
            QVERIFY(tile->orthotexture.width() == 256);
            QVERIFY(tile->orthotexture.height() == 256);
        }
    }

    void emitsReceivedTilesWhenSomeAreUnavailable()
    {
        m_unavailable_tiles.insert(tile::Id { .zoom_level = 0, .coords = { 0, 0 } });
        m_unavailable_tiles.insert(tile::Id { .zoom_level = 1, .coords = { 0, 0 } });
        m_unavailable_tiles.insert(tile::Id { .zoom_level = 1, .coords = { 0, 1 } });
        m_unavailable_tiles.insert(tile::Id { .zoom_level = 1, .coords = { 1, 0 } });
        m_unavailable_tiles.insert(tile::Id { .zoom_level = 1, .coords = { 1, 1 } });

        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);
        connect(this, &TestTileScheduler::tileUnavailable, m_scheduler.get(), &TileScheduler::notify_about_unavailable_ortho_tile);
        connect(this, &TestTileScheduler::tileUnavailable, m_scheduler.get(), &TileScheduler::notify_about_unavailable_height_tile);
        QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_ready);
        m_scheduler->update_camera(test_cam);
        spy.wait(10);
        QVERIFY(m_given_tiles.size() >= 10);
        QVERIFY(size_t(spy.size()) == m_given_tiles.size());
        for (const QList<QVariant>& signal : spy) { // yes, QSignalSpy is a QList<QList<QVariant>>, where the inner QList contains the signal arguments
            const std::shared_ptr<Tile> tile = signal.at(0).value<std::shared_ptr<Tile>>();
            QVERIFY(tile);
            QVERIFY(m_given_tiles.contains(tile->id));
            QVERIFY(tile->height_map.width() == 256);
            QVERIFY(tile->height_map.height() == 256);
            QVERIFY(tile->orthotexture.width() == 256);
            QVERIFY(tile->orthotexture.height() == 256);
        }
    }

    void freesMemory()
    {
        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);
        m_scheduler->update_camera(test_cam);
        QTest::qWait(10);
        QVERIFY(m_scheduler->number_of_tiles_in_transit() == 0);
        QVERIFY(m_scheduler->number_of_waiting_height_tiles() == 0);
        QVERIFY(m_scheduler->number_of_waiting_ortho_tiles() == 0);
    }

    void hasInfoAboutGpuTiles()
    {
        QVERIFY(m_scheduler->gpu_tiles().empty());
        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);
        m_scheduler->update_camera(test_cam);
        QTest::qWait(10);
        const auto gpu_tiles = m_scheduler->gpu_tiles();
        QVERIFY(gpu_tiles.size() == m_given_tiles.size());
        for (const auto& t : gpu_tiles) {
            QVERIFY(m_given_tiles.contains(t));
        }
    }

    void doesntRequestTilesWhichAreOnTheGpu()
    {
        QVERIFY(m_scheduler->gpu_tiles().empty());
        connect(m_scheduler.get(), &TileScheduler::tile_requested, this, &TestTileScheduler::giveTiles);
        connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receive_ortho_tile);
        connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receive_height_tile);
        m_scheduler->update_camera(test_cam);
        QTest::qWait(10);
        // tiles are on the gpu

        QSignalSpy spy(m_scheduler.get(), &TileScheduler::tile_requested);
        QVERIFY(m_scheduler->number_of_tiles_in_transit() == 0);
        m_scheduler->update_camera(test_cam);
        spy.wait(5);
        QVERIFY(spy.empty());
    }
};

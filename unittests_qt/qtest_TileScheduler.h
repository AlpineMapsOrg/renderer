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

#include "alpine_renderer/TileScheduler.h"

#include <unordered_set>

#include <QTest>
#include <QSignalSpy>
#include <glm/glm.hpp>

#include "alpine_renderer/Camera.h"
#include "alpine_renderer/srs.h"
#include "alpine_renderer/Tile.h"

class TestTileScheduler: public QObject
{
  Q_OBJECT
private:
  QByteArray m_ortho_bytes;
  QByteArray m_height_bytes;
  std::unique_ptr<TileScheduler> m_scheduler;
  Camera test_cam = Camera({1822577.0, 6141664.0 - 500, 171.28 + 500}, {1822577.0, 6141664.0, 171.28}); // should point right at the stephansdom
  std::unordered_set<srs::TileId, srs::TileId::Hasher> m_given_tiles;

  virtual std::unique_ptr<TileScheduler> makeScheduler() const = 0;


public slots:
  void giveTiles(const srs::TileId& tile_id) {
    m_given_tiles.insert(tile_id);
    emit orthoTileReady(tile_id, std::make_shared<QByteArray>(m_ortho_bytes));
    emit heightTileReady(tile_id, std::make_shared<QByteArray>(m_height_bytes));
  }

signals:
  void orthoTileReady(srs::TileId tile_id, std::shared_ptr<QByteArray> data);
  void heightTileReady(srs::TileId tile_id, std::shared_ptr<QByteArray> data);

private slots:
  void initTestCase() {
    auto ortho_file = QFile(QString("%1%2").arg(ATB_TEST_DATA_DIR, "test-tile_ortho.jpeg"));
    ortho_file.open(QFile::ReadOnly);
    m_ortho_bytes = ortho_file.readAll();
    QVERIFY(m_ortho_bytes.size() > 10);

    auto height_file = QFile(QString("%1%2").arg(ATB_TEST_DATA_DIR, "test-tile.png"));
    height_file.open(QFile::ReadOnly);
    m_height_bytes = height_file.readAll();
    QVERIFY(m_height_bytes.size() > 10);
  }

  void init() {
    m_scheduler = makeScheduler();
    m_given_tiles.clear();
  }

  void emitsTileRequestsWhenUpdatingCamera() {
    QSignalSpy spy(m_scheduler.get(), &TileScheduler::tileRequested);
    QVERIFY(m_scheduler->numberOfTilesInTransit() == 0);
    m_scheduler->updateCamera(test_cam);
    spy.wait(5);

    QVERIFY(spy.size() >= 1);
    QVERIFY(m_scheduler->numberOfTilesInTransit() == size_t(spy.size()));
    auto n_tiles_containing_camera_position = 0;
    auto n_tiles_containing_camera_view_dir = 0;
    for (const QList<QVariant>& signal : spy) {    // yes, QSignalSpy is a QList<QList<QVariant>>, where the inner QList contains the signal arguments
      const auto tile_id = signal.at(0).value<srs::TileId>();
      QVERIFY(tile_id.zoom_level < 30);
      const auto tile_bounds = srs::tile_bounds(tile_id);
      if (contains(tile_bounds, glm::dvec2(test_cam.position())))
        n_tiles_containing_camera_position++;
      if (contains(tile_bounds, {1822577.0, 6141664.0}))
        n_tiles_containing_camera_view_dir++;
    }
    QCOMPARE(n_tiles_containing_camera_view_dir, 1);
    QCOMPARE(n_tiles_containing_camera_position, 1);
  }

  void tileRequestsSentOnlyOnce() {
    {
      QSignalSpy spy(m_scheduler.get(), &TileScheduler::tileRequested);
      m_scheduler->updateCamera(test_cam);
      spy.wait(10); // ignore first batch of requests
    }
    {
      QSignalSpy spy(m_scheduler.get(), &TileScheduler::tileRequested);
      m_scheduler->updateCamera(test_cam);
      spy.wait(10);
      QVERIFY(spy.count() == 0);  // the scheduler should know, that it already requested these tiles
    }
  }

  void emitsLoadedTiles() {
    connect(m_scheduler.get(), &TileScheduler::tileRequested, this, &TestTileScheduler::giveTiles);
    connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receiveOrthoTile);
    connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receiveHeightTile);
    QSignalSpy spy(m_scheduler.get(), &TileScheduler::tileReady);
    m_scheduler->updateCamera(test_cam);
    spy.wait(10);
    QVERIFY(size_t(spy.size()) == m_given_tiles.size());
    for (const QList<QVariant>& signal : spy) {    // yes, QSignalSpy is a QList<QList<QVariant>>, where the inner QList contains the signal arguments
      const std::shared_ptr<Tile> tile = signal.at(0).value<std::shared_ptr<Tile>>();
      QVERIFY(tile);
      QVERIFY(m_given_tiles.contains(tile->id));
      QVERIFY(tile->height_map.width() == 256);
      QVERIFY(tile->height_map.height() == 256);
      QVERIFY(tile->orthotexture.width() == 256);
      QVERIFY(tile->orthotexture.height() == 256);
    }
  }
  void freesMemory() {
    connect(m_scheduler.get(), &TileScheduler::tileRequested, this, &TestTileScheduler::giveTiles);
    connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receiveOrthoTile);
    connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receiveHeightTile);
    m_scheduler->updateCamera(test_cam);
    QTest::qWait(10);
    QVERIFY(m_scheduler->numberOfTilesInTransit() == 0);
    QVERIFY(m_scheduler->numberOfWaitingHeightTiles() == 0);
    QVERIFY(m_scheduler->numberOfWaitingOrthoTiles() == 0);
  }

  void hasInfoAboutGpuTiles() {
    QVERIFY(m_scheduler->gpuTiles().empty());
    connect(m_scheduler.get(), &TileScheduler::tileRequested, this, &TestTileScheduler::giveTiles);
    connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receiveOrthoTile);
    connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receiveHeightTile);
    m_scheduler->updateCamera(test_cam);
    QTest::qWait(10);
    const auto gpu_tiles = m_scheduler->gpuTiles();
    QVERIFY(gpu_tiles.size() == m_given_tiles.size());
    for (const auto& t : gpu_tiles) {
      QVERIFY(m_given_tiles.contains(t));
    }
  }

  void doesntRequestTilesWhichAreOnTheGpu() {
    QVERIFY(m_scheduler->gpuTiles().empty());
    connect(m_scheduler.get(), &TileScheduler::tileRequested, this, &TestTileScheduler::giveTiles);
    connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receiveOrthoTile);
    connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receiveHeightTile);
    m_scheduler->updateCamera(test_cam);
    QTest::qWait(10);
    // tiles are on the gpu

    QSignalSpy spy(m_scheduler.get(), &TileScheduler::tileRequested);
    QVERIFY(m_scheduler->numberOfTilesInTransit() == 0);
    m_scheduler->updateCamera(test_cam);
    spy.wait(5);
    QVERIFY(spy.empty());
  }


  void expiresOldTiles() {
    QVERIFY(m_scheduler->gpuTiles().empty());
    connect(m_scheduler.get(), &TileScheduler::tileRequested, this, &TestTileScheduler::giveTiles);
    connect(this, &TestTileScheduler::orthoTileReady, m_scheduler.get(), &TileScheduler::receiveOrthoTile);
    connect(this, &TestTileScheduler::heightTileReady, m_scheduler.get(), &TileScheduler::receiveHeightTile);
    m_scheduler->updateCamera(test_cam);
    QTest::qWait(10);
    // tiles are on the gpu
    const auto gpu_tiles = m_scheduler->gpuTiles();

    QSignalSpy spy(m_scheduler.get(), &TileScheduler::tileExpired);
    Camera replacement_cam = Camera({0.0, 0.0 - 500, 0.0 - 500}, {0.0, 0.0, -1000.0});
    m_scheduler->updateCamera(replacement_cam);
    spy.wait(5);
    QVERIFY(m_scheduler->gpuTiles().size() == 1);   // only root tile
    QVERIFY(size_t(spy.size()) == gpu_tiles.size());
    for (const auto& tileExpireSignal : spy) {
      const srs::TileId tile = tileExpireSignal.at(0).value<srs::TileId>();
      QVERIFY(gpu_tiles.contains(tile));
    }
  }
};

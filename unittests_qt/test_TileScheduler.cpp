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

#include "render_backend/TileScheduler.h"
#include <QTest>
#include <QSignalSpy>
#include <glm/glm.hpp>

#include "render_backend/Camera.h"
#include "render_backend/srs.h"


class TestTileScheduler: public QObject
{
  Q_OBJECT


private slots:
  void loadCandidates() {
    TileScheduler scheduler;
    Camera test_cam = Camera({1822577.0, 6141664.0 - 500, 171.28 + 500}, {1822577.0, 6141664.0, 171.28}); // should point right at the stephansdom
    const auto tile_list = scheduler.loadCandidates(test_cam);
    QVERIFY(!tile_list.empty());
  }

  void emitsTileRequestsWhenCalled() {
    TileScheduler scheduler;
    Camera test_cam = Camera({1822577.0, 6141664.0 - 500, 171.28 + 500}, {1822577.0, 6141664.0, 171.28}); // should point right at the stephansdom
    QSignalSpy spy(&scheduler, &TileScheduler::tileRequested);
    scheduler.updateCamera(test_cam);
    spy.wait(5);

    QVERIFY(spy.count() >= 1);
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
};


QTEST_MAIN(TestTileScheduler)
#include "test_TileScheduler.moc"

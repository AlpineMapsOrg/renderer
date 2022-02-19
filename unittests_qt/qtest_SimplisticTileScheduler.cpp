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

#include "render_backend/SimplisticTileScheduler.h"
#include "unittests_qt/qtest_TileScheduler.h"

#include <unordered_set>

#include <QTest>
#include <QSignalSpy>
#include <glm/glm.hpp>

#include "render_backend/Camera.h"
#include "render_backend/srs.h"
#include "render_backend/Tile.h"


class TestSimplisticTileScheduler: public TestTileScheduler
{
  Q_OBJECT
private:
  Camera test_cam = Camera({1822577.0, 6141664.0 - 500, 171.28 + 500}, {1822577.0, 6141664.0, 171.28}); // should point right at the stephansdom

  std::unique_ptr<TileScheduler> makeScheduler() const override {
    return std::make_unique<SimplisticTileScheduler>();
  }

private slots:
//  void initTestCase() {}    // implementing these functions will override TestTileScheduler and break the tests.
//  void init() {}            // so call the TestTileScheduler::init and initTestCase somehow, then it should be good again.
  void loadCandidates() {
    const auto tile_list = SimplisticTileScheduler::loadCandidates(test_cam);
    QVERIFY(!tile_list.empty());
  }
};


QTEST_MAIN(TestSimplisticTileScheduler)
#include "qtest_SimplisticTileScheduler.moc"

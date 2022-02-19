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

#include "alpine_renderer/tile_scheduler/BasicTreeTileScheduler.h"
#include "unittests_qt/qtest_TileScheduler.h"

#include <unordered_set>

#include <QTest>
#include <QSignalSpy>
#include <glm/glm.hpp>

#include "alpine_renderer/Camera.h"
#include "alpine_renderer/srs.h"
#include "alpine_renderer/Tile.h"


class TestBasicTreeTileScheduler: public TestTileScheduler
{
  Q_OBJECT
private:
  Camera test_cam = Camera({1822577.0, 6141664.0 - 500, 171.28 + 500}, {1822577.0, 6141664.0, 171.28}); // should point right at the stephansdom

  std::unique_ptr<TileScheduler> makeScheduler() const override {
    return std::make_unique<BasicTreeTileScheduler>();
  }

private slots:
//  void initTestCase() {}    // implementing these functions will override TestTileScheduler and break the tests.
//  void init() {}            // so call the TestTileScheduler::init and initTestCase somehow, then it should be good again.

};


QTEST_MAIN(TestBasicTreeTileScheduler)
#include "qtest_BasicTreeTileScheduler.moc"

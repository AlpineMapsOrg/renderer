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

class TestTileScheduler: public QObject
{
  Q_OBJECT
private slots:
  void toUpper() {
    QString str = "Hello";
    QVERIFY(str.toUpper() == "HELLO");
  }
};


QTEST_MAIN(TestTileScheduler)
#include "test_TileScheduler.moc"

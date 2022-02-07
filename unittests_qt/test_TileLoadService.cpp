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

#include "render_backend/TileLoadService.h"

#include <QTest>
#include <QSignalSpy>

class TestTileLoadService: public QObject
{
  Q_OBJECT
private slots:
  void download() {
    // https://maps2.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/9/177/273.jpeg => should be a white tile
    // https://maps2.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/9/179/272.jpeg => should show Tirol
    TileLoadService service("https://maps2.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZXY, ".jpeg");
    QSignalSpy spy(&service, &TileLoadService::loadReady);
    service.load("project-logos/enwiki.png");

    spy.wait(250);

    QCOMPARE(spy.count(), 1); // make sure the signal was emitted exactly one time
//    QList<QVariant> arguments = spy.takeFirst(); // take the first signal

//    QVERIFY(arguments.at(0).toByteArray()); // verify the first argument
  }
};


QTEST_MAIN(TestTileLoadService)
#include "test_TileLoadService.moc"

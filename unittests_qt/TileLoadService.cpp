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

#include "nucleus/TileLoadService.h"
#include "nucleus/utils/tile_conversion.h"
#include <algorithm>

#include <QSignalSpy>
#include <QTest>

class TestTileLoadService : public QObject {
    Q_OBJECT
private slots:
    void buildTileUrl()
    {
        {
            TileLoadService service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZXY, ".jpeg");
            QCOMPARE(service.build_tile_url({ .zoom_level = 2, .coords = { 1, 3 } }),
                "https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/2/1/3.jpeg");
        }
        {
            TileLoadService service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZYX, ".jpeg");
            QCOMPARE(service.build_tile_url({ .zoom_level = 2, .coords = { 1, 3 } }),
                "https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/2/3/1.jpeg");
        }
        {
            TileLoadService service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZXY_yPointingSouth, ".jpeg");
            QCOMPARE(service.build_tile_url({ .zoom_level = 2, .coords = { 1, 3 } }),
                "https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/2/1/0.jpeg");
        }
        {
            TileLoadService service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg");
            QCOMPARE(service.build_tile_url({ .zoom_level = 2, .coords = { 1, 3 } }),
                "https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/2/0/1.jpeg");
        }
    }

    void download()
    {
        // https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/9/177/273.jpeg => should be a white tile
        // https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/9/179/272.jpeg => should show Tirol
        const auto white_tile_id = tile::Id { .zoom_level = 9, .coords = { 273, 177 } };
        const auto tirol_tile_id = tile::Id { .zoom_level = 9, .coords = { 272, 179 } };
        TileLoadService service("http://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZYX, ".jpeg");

        {
            QSignalSpy spy(&service, &TileLoadService::loadReady);
            service.load(white_tile_id);
            spy.wait(250);

            QCOMPARE(spy.count(), 1); // make sure the signal was emitted exactly one time
            QList<QVariant> arguments = spy.takeFirst(); // take the first signal
            QCOMPARE(arguments.at(0).value<tile::Id>().zoom_level, white_tile_id.zoom_level); // verify the first argument
            QCOMPARE(arguments.at(0).value<tile::Id>().coords, white_tile_id.coords); // verify the first argument
            const auto image_bytes = arguments.at(1).value<std::shared_ptr<QByteArray>>();
            const auto image = tile_conversion::toQImage(*image_bytes);
            QVERIFY(image.sizeInBytes()); // verify the first argument
            // the image on the server is only almost white. this test will fail when the file changes.
            QCOMPARE(256LLu * 256 * 255 * 4 - std::accumulate(image.constBits(), image.constBits() + image.sizeInBytes(), 0LLu), 342792);
        }
        {
            QSignalSpy spy(&service, &TileLoadService::loadReady);
            service.load(tirol_tile_id);
            spy.wait(250);

            QCOMPARE(spy.count(), 1); // make sure the signal was emitted exactly one time
            QList<QVariant> arguments = spy.takeFirst(); // take the first signal
            QCOMPARE(arguments.at(0).value<tile::Id>().zoom_level, tirol_tile_id.zoom_level); // verify the first argument
            QCOMPARE(arguments.at(0).value<tile::Id>().coords, tirol_tile_id.coords); // verify the first argument
            const auto image_bytes = arguments.at(1).value<std::shared_ptr<QByteArray>>();
            const auto image = tile_conversion::toQImage(*image_bytes);
            QVERIFY(image.sizeInBytes()); // verify the first argument
            // manually checked. comparing the sum should find regressions. this test will fail when the file changes.
            image.save("/home/madam/Documents/work/tuw/alpinemaps/build-alpine-renderer-Desktop_Qt_6_2_3_GCC_64bit-Debug/test.jpeg");
            QCOMPARE(std::accumulate(image.constBits(), image.constBits() + image.sizeInBytes(), 0LLu), 34880685LLu); // don't know what it will sum up to, but certainly not zero..
        }
    }

    void notifiesOfUnavailableTiles()
    {
        TileLoadService service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TileLoadService::UrlPattern::ZYX, ".jpeg");
        QSignalSpy spy(&service, &TileLoadService::tileUnavailable);
        tile::Id unavailable_tile_id = { .zoom_level = 90, .coords = { 273, 177 } };
        service.load(unavailable_tile_id);
        spy.wait(250);
        QCOMPARE(spy.count(), 1);
        QList<QVariant> arguments = spy.takeFirst(); // take the first signal
        QCOMPARE(arguments.at(0).value<tile::Id>(), unavailable_tile_id); // verify the first argument
    }
};

QTEST_MAIN(TestTileLoadService)
#include "TileLoadService.moc"

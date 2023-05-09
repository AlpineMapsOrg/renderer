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


#include <algorithm>

#include <QRegularExpression>
#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>

#include "nucleus/tile_scheduler/TileLoadService.h"
#include "nucleus/utils/tile_conversion.h"

using namespace nucleus::tile_scheduler;

inline std::ostream& operator<<(std::ostream& os, const QString& value)
{
    os << value.toStdString();
    return os;
}


TEST_CASE("nucleus/tile_scheduler/TileLoadService")
{
    SECTION("build tile url")
    {
        {
            TileLoadService
                service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                        TileLoadService::UrlPattern::ZXY,
                        ".jpeg");
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}})
                  == "https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/2/1/"
                     "3.jpeg");
        }
        {
            TileLoadService
                service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                        TileLoadService::UrlPattern::ZYX,
                        ".jpeg");
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}})
                  == "https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/2/3/"
                     "1.jpeg");
        }
        {
            TileLoadService
                service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                        TileLoadService::UrlPattern::ZXY_yPointingSouth,
                        ".jpeg");
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}})
                  == "https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/2/1/"
                     "0.jpeg");
        }
        {
            TileLoadService
                service("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                        TileLoadService::UrlPattern::ZYX_yPointingSouth,
                        ".jpeg");
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}})
                  == "https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/2/0/"
                     "1.jpeg");
        }
    }

    SECTION("build tile url with load balancing")
    {
        {
            TileLoadService service("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                        TileLoadService::UrlPattern::ZXY,
                        ".jpeg",
                        {"1", "2", "3", "4"});
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}) == service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}));
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}).contains(QRegularExpression("https://maps[1-4]\\.wien\\.gv\\.at/basemap/bmaporthofoto30cm/normal/google3857/2/1/3.jpeg")));
        }
        {
            TileLoadService service("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                        TileLoadService::UrlPattern::ZYX,
                        ".jpeg",
                        {"1", "2", "3", "4"});
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}) == service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}));
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}).contains(QRegularExpression("https://maps[1-4]\\.wien\\.gv\\.at/basemap/bmaporthofoto30cm/normal/google3857/2/3/1.jpeg")));
        }
        {
            TileLoadService service("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                        TileLoadService::UrlPattern::ZXY_yPointingSouth,
                        ".jpeg",
                        {"1", "2", "3", "4"});
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}) == service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}));
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}).contains(QRegularExpression("https://maps[1-4]\\.wien\\.gv\\.at/basemap/bmaporthofoto30cm/normal/google3857/2/1/0.jpeg")));
        }
        {
            TileLoadService service("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                        TileLoadService::UrlPattern::ZYX_yPointingSouth,
                        ".jpeg",
                        {"1", "2", "3", "4"});
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}) == service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}));
            CHECK(service.build_tile_url({.zoom_level = 2, .coords = {1, 3}}).contains(QRegularExpression("https://maps[1-4]\\.wien\\.gv\\.at/basemap/bmaporthofoto30cm/normal/google3857/2/0/1.jpeg")));
        }
    }

    SECTION("download")
    {
        // https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/9/177/273.jpeg => should be a white tile
        // https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/9/179/272.jpeg => should show Tirol
        const auto white_tile_id = tile::Id{.zoom_level = 9, .coords = {273, 177}};
        const auto tirol_tile_id = tile::Id{.zoom_level = 9, .coords = {272, 179}};
        TileLoadService
            service("https://mapsneu.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                    TileLoadService::UrlPattern::ZYX,
                    ".jpeg");

        {
            QSignalSpy spy(&service, &TileLoadService::load_ready);
            service.load(white_tile_id);
            spy.wait(10000);

            REQUIRE(spy.count() == 1); // make sure the signal was emitted exactly one time
            QList<QVariant> arguments = spy.takeFirst(); // take the first signal
            CHECK(arguments.at(0).value<tile::Id>().zoom_level
                  == white_tile_id.zoom_level); // verify the first argument
            CHECK(arguments.at(0).value<tile::Id>().coords
                  == white_tile_id.coords); // verify the first argument
            const auto image_bytes = arguments.at(1).value<std::shared_ptr<QByteArray>>();
            const auto image = nucleus::utils::tile_conversion::toQImage(*image_bytes);
            REQUIRE(image.sizeInBytes() > 0); // verify the first argument
            // the image on the server is only almost white. this test will fail when the file changes.
            CHECK(256LLu * 256 * 255 * 4
                      - std::accumulate(image.constBits(),
                                        image.constBits() + image.sizeInBytes(),
                                        0LLu)
                  == 342792);
        }
        {
            QSignalSpy spy(&service, &TileLoadService::load_ready);
            service.load(tirol_tile_id);
            spy.wait(10000);

            REQUIRE(spy.count() == 1); // make sure the signal was emitted exactly one time
            QList<QVariant> arguments = spy.takeFirst(); // take the first signal
            CHECK(arguments.at(0).value<tile::Id>().zoom_level
                  == tirol_tile_id.zoom_level); // verify the first argument
            CHECK(arguments.at(0).value<tile::Id>().coords
                  == tirol_tile_id.coords); // verify the first argument
            const auto image_bytes = arguments.at(1).value<std::shared_ptr<QByteArray>>();
            const auto image = nucleus::utils::tile_conversion::toQImage(*image_bytes);
            REQUIRE(image.sizeInBytes() > 0); // verify the first argument
            // manually checked. comparing the sum should find regressions. this test will fail when the file changes.
//            image.save("/home/madam/Documents/work/tuw/alpinemaps/"
//                       "build-alpine-renderer-Desktop_Qt_6_2_3_GCC_64bit-Debug/test.jpeg");
            CHECK(std::accumulate(image.constBits(), image.constBits() + image.sizeInBytes(), 0LLu) == 34880685LLu); // don't know what it will sum up to, but certainly not zero..
        }
    }

    SECTION("notifies of unavailable tiles")
    {
        TileLoadService service("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/",
                                TileLoadService::UrlPattern::ZYX,
                                ".png");
        QSignalSpy spy(&service, &TileLoadService::tile_unavailable);
        QSignalSpy spy2(&service, &TileLoadService::load_ready);
        tile::Id unavailable_tile_id = {.zoom_level = 90, .coords = {273, 177}};
        service.load(unavailable_tile_id);
        spy.wait(10000);
        CHECK(spy2.count() == 0);
        REQUIRE(spy.count() == 1);
        QList<QVariant> arguments = spy.takeFirst();                     // take the first signal
        CHECK(arguments.at(0).value<tile::Id>() == unavailable_tile_id); // verify the first argument
    }

    SECTION("notifies of timeouts")
    {
        TileLoadService service("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/",
            TileLoadService::UrlPattern::ZYX,
            ".png");
        service.set_transfer_timeout(1);
        QSignalSpy spy(&service, &TileLoadService::tile_unavailable);
        tile::Id unavailable_tile_id = { .zoom_level = 90, .coords = { 273, 177 } };
        service.load(unavailable_tile_id);
        spy.wait(20);
        REQUIRE(spy.count() == 1);
        QList<QVariant> arguments = spy.takeFirst(); // take the first signal
        CHECK(arguments.at(0).value<tile::Id>() == unavailable_tile_id); // verify the first argument
    }
}

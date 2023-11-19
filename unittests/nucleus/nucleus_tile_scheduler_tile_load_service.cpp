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
using nucleus::tile_scheduler::tile_types::TileLayer;

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

    SECTION("network network info struct") {
        using nucleus::tile_scheduler::tile_types::NetworkInfo;
        {
            const auto joined = NetworkInfo::join(NetworkInfo{NetworkInfo::Status::Good, 1}, NetworkInfo{NetworkInfo::Status::NotFound, 2});
            CHECK(joined.status == NetworkInfo::Status::NotFound);
            CHECK(joined.timestamp == 1);
        }
        {
            const auto joined = NetworkInfo::join(NetworkInfo{NetworkInfo::Status::Good, 4},
                                                  NetworkInfo{NetworkInfo::Status::NotFound, 3},
                                                  NetworkInfo{NetworkInfo::Status::NetworkError, 2});
            CHECK(joined.status == NetworkInfo::Status::NetworkError);
            CHECK(joined.timestamp == 2);
        }
        {
            const auto joined = NetworkInfo::join(NetworkInfo{NetworkInfo::Status::NetworkError, 4},
                                                  NetworkInfo{NetworkInfo::Status::NotFound, 3},
                                                  NetworkInfo{NetworkInfo::Status::Good, 2});
            CHECK(joined.status == NetworkInfo::Status::NetworkError);
            CHECK(joined.timestamp == 2);
        }
    }

    SECTION("download")
    {
        // https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/9/177/273.jpeg => should be a white tile
        // https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/9/179/272.jpeg => should show Tirol
        const auto white_tile_id = tile::Id{.zoom_level = 9, .coords = {273, 177}};
        const auto tirol_tile_id = tile::Id{.zoom_level = 9, .coords = {272, 179}};
        TileLoadService service("https://mapsneu.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
                                TileLoadService::UrlPattern::ZYX,
                                ".jpeg");

        {
            QSignalSpy spy(&service, &TileLoadService::load_finished);
            service.load(white_tile_id);
            spy.wait(10000);

            REQUIRE(spy.count() == 1);
            QList<QVariant> arguments = spy.takeFirst();
            REQUIRE(arguments.size() == 1);
            TileLayer tile = arguments.at(0).value<TileLayer>();
            CHECK(tile.id == white_tile_id);
            CHECK(tile.network_info.status == tile_types::NetworkInfo::Status::Good);
            CHECK(utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

            const auto image = nucleus::utils::tile_conversion::toQImage(*tile.data);
            REQUIRE(image.sizeInBytes() > 0);
            // the image on the server is only almost white. this test will fail when the file changes.
            CHECK(std::accumulate(image.constBits(), image.constBits() + image.sizeInBytes(), 0LLu) == 66'503'928LLu);
        }
        {
            QSignalSpy spy(&service, &TileLoadService::load_finished);
            service.load(tirol_tile_id);
            spy.wait(10000);

            REQUIRE(spy.count() == 1);
            QList<QVariant> arguments = spy.takeFirst();
            REQUIRE(arguments.size() == 1);
            const auto tile = arguments.at(0).value<TileLayer>();
            CHECK(tile.id == tirol_tile_id);
            CHECK(tile.network_info.status == tile_types::NetworkInfo::Status::Good);
            CHECK(utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

            const auto image = nucleus::utils::tile_conversion::toQImage(*tile.data);
            REQUIRE(image.sizeInBytes() > 0);
            // manually checked. comparing the sum should find regressions. this test will fail when the file changes.
//            image.save("/home/madam/Documents/work/tuw/alpinemaps/"
//                       "build-alpine-renderer-Desktop_Qt_6_2_3_GCC_64bit-Debug/test.jpeg");
            CHECK(std::accumulate(image.constBits(), image.constBits() + image.sizeInBytes(), 0LLu) == 36'889'463LLu);
        }
    }
#ifndef __EMSCRIPTEN__
    // this one doesn't work in emscripten, because 404s often also cause cors errors, which qt doesn't see.
    SECTION("notifies of unavailable tiles")
    {
        TileLoadService service("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/",
                                TileLoadService::UrlPattern::ZYX,
                                ".png");
        QSignalSpy spy(&service, &TileLoadService::load_finished);
        tile::Id unavailable_tile_id = {.zoom_level = 90, .coords = {273, 177}};
        service.load(unavailable_tile_id);
        spy.wait(10000);

        REQUIRE(spy.count() == 1);
        QList<QVariant> arguments = spy.takeFirst();
        REQUIRE(arguments.size() == 1);
        const auto tile = arguments.at(0).value<TileLayer>();
        CHECK(tile.id == unavailable_tile_id);
        CHECK(tile.network_info.status == tile_types::NetworkInfo::Status::NotFound);
        CHECK(utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

        const auto image = nucleus::utils::tile_conversion::toQImage(*tile.data);
        REQUIRE(image.sizeInBytes() == 0);
    }
#endif

    SECTION("notifies of network error")
    {
        TileLoadService service("https://bad_url_23a9sd25fds87jcs6k43l.at/tiles/alpine_png/",
                                TileLoadService::UrlPattern::ZYX,
                                ".png");
        QSignalSpy spy(&service, &TileLoadService::load_finished);
        tile::Id unavailable_tile_id = {.zoom_level = 90, .coords = {273, 177}};
        service.load(unavailable_tile_id);
        spy.wait(10000);

        REQUIRE(spy.count() == 1);
        QList<QVariant> arguments = spy.takeFirst();
        REQUIRE(arguments.size() == 1);
        const auto tile = arguments.at(0).value<TileLayer>();
        CHECK(tile.id == unavailable_tile_id);
        CHECK(tile.network_info.status == tile_types::NetworkInfo::Status::NetworkError);
        CHECK(utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

        const auto image = nucleus::utils::tile_conversion::toQImage(*tile.data);
        REQUIRE(image.sizeInBytes() == 0);
    }

    SECTION("notifies of timeout")
    {
        TileLoadService service("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/",
                                TileLoadService::UrlPattern::ZYX,
                                ".png");
        service.set_transfer_timeout(1);
        QSignalSpy spy(&service, &TileLoadService::load_finished);
        tile::Id unavailable_tile_id = {.zoom_level = 90, .coords = {273, 177}};
        service.load(unavailable_tile_id);
        spy.wait(20);

        REQUIRE(spy.count() == 1);
        QList<QVariant> arguments = spy.takeFirst();
        REQUIRE(arguments.size() == 1);
        const auto tile = arguments.at(0).value<TileLayer>();
        CHECK(tile.id == unavailable_tile_id);
        CHECK(tile.network_info.status == tile_types::NetworkInfo::Status::NetworkError);
        CHECK(utils::time_since_epoch() - tile.network_info.timestamp < 20);

        const auto image = nucleus::utils::tile_conversion::toQImage(*tile.data);
        REQUIRE(image.sizeInBytes() == 0);
    }
}

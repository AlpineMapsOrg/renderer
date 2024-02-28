/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include <QDebug>
#include <QFile>
#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "nucleus/tile_scheduler/TileLoadService.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/vector_tiles/VectorTileFeature.h"
#include "nucleus/vector_tiles/VectorTileManager.h"
#include "radix/tile.h"

TEST_CASE("nucleus/vector_tiles")
{
    SECTION("Tile does not exist yet")
    {
        tile::Id id(13, glm::uvec2(4384, 2878), tile::Scheme::Tms);
        nucleus::VectorTileManager v;

        v.get_tile(id);

        const auto tileData = v.get_tile(id);
        CHECK(tileData.size() == 0);
    }

    SECTION("PBF parsing")
    {
        nucleus::VectorTileManager v;

        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "vectortile.mvt");
        QFile file(filepath);
        file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QByteArray data = file.readAll();

        CHECK(data.size() > 0);

        tile::Id id(13, glm::uvec2(4384, 2878), tile::Scheme::Tms);

        nucleus::tile_scheduler::tile_types::TileLayer tile { id, { nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good, nucleus::tile_scheduler::utils::time_since_epoch() }, std::make_shared<QByteArray>(data) };

        v.deliver_vectortile(tile);

        const auto& tileData = v.get_tile(tile.id);

        CHECK(tileData.size() == 16);

        for (const auto& peak : tileData) {

            if (peak->id == 26863041ul) {
                // Check if großglockner has been successfully parsed (note the id might change in the future)
                // CHECK(peak->elevation == 3798);
                CHECK(peak->name == "Großglockner");
            }
            // peak->print();
        }
    }

    SECTION("Tile download")
    {
        const auto id = tile::Id { .zoom_level = 13, .coords = { 4384, 2878 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

        nucleus::tile_scheduler::TileLoadService service(nucleus::VectorTileManager::TILE_SERVER, nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, ".mvt");
        // std::cout << "loading: " << qUtf8Printable(service.build_tile_url(id)) << std::endl;
        {
            QSignalSpy spy(&service, &nucleus::tile_scheduler::TileLoadService::load_finished);
            service.load(id);
            spy.wait(10000);

            REQUIRE(spy.count() == 1);
            QList<QVariant> arguments = spy.takeFirst();
            REQUIRE(arguments.size() == 1);
            nucleus::tile_scheduler::tile_types::TileLayer tile = arguments.at(0).value<nucleus::tile_scheduler::tile_types::TileLayer>();
            CHECK(tile.id == id);
            CHECK(tile.network_info.status == nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good);
            CHECK(nucleus::tile_scheduler::utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

            QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "vectortile.mvt");
            QFile file(filepath);
            file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
            QByteArray data = file.readAll();

            REQUIRE(tile.data->size() > 0);
            CHECK(tile.data->size() == data.size());
            CHECK(*tile.data == data);
        }
    }

    SECTION("Tile download parent")
    {
        nucleus::tile_scheduler::TileLoadService service(nucleus::VectorTileManager::TILE_SERVER, nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, ".mvt");

        nucleus::VectorTileManager v;

        auto id = tile::Id { .zoom_level = 13, .coords = { 4384, 2878 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

        for (int i = 13; i > 9; i--) {
            std::cout << "loading: " << qUtf8Printable(service.build_tile_url(id)) << std::endl;

            QSignalSpy spy(&service, &nucleus::tile_scheduler::TileLoadService::load_finished);
            service.load(id);
            spy.wait(10000);

            REQUIRE(spy.count() == 1);
            QList<QVariant> arguments = spy.takeFirst();
            REQUIRE(arguments.size() == 1);
            const auto tile = arguments.at(0).value<nucleus::tile_scheduler::tile_types::TileLayer>();
            CHECK(tile.id == id);
            CHECK(tile.network_info.status == nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good);
            CHECK(nucleus::tile_scheduler::utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

            // std::cout << "here: " << tile.data->size() << std::endl;

            REQUIRE(tile.data->size() > 0);

            v.deliver_vectortile(tile);
            const auto& tile_features = v.get_tile(id);

            // TODO add checks to see if the data / parsing is still correct

            std::cout << "features: " << tile_features.size() << std::endl;
            for (const auto& feature : tile_features) {
                feature->print();
            }

            id = id.parent();
        }
    }
}

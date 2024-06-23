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

//#include "nucleus/tile_scheduler/TileLoadService.h"
//#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/vector_tiles/VectorTileFeature.h"
//#include "nucleus/vector_tiles/VectorTileManager.h"
//#include "radix/tile.h"
#include "nucleus/map_label/Charset.h"

#include <algorithm>
#include <set>


TEST_CASE("nucleus/vector_tiles")
{





    // Checks if reading the charset file and creating the char set works
    SECTION("charset reading")
    {
        nucleus::maplabel::Charset& c = nucleus::maplabel::Charset::get_instance();

        std::set<char16_t> all_chars = c.get_all_chars();

        // is the number of chars correct
        CHECK(all_chars.size() == 123);
        // does the set contain certain chars (e.g. " " -> 32)
        CHECK(all_chars.contains(32));
    }




//    SECTION("char insert/compare")
//    {
//        std::set<char16_t> all_char_list = {};

//        nucleus::maplabel::Charset& c = nucleus::maplabel::Charset::get_instance();

//        auto cached_chars = c.get_all_chars();
//        all_char_list.insert(cached_chars.begin(), cached_chars.end());

//        std::cout << cached_chars.size() << std::endl;

//        // check if test value is not contained in the set
//        CHECK(!all_char_list.contains(28779));
//        all_char_list.insert(28779); // insert this test value to the set

//        std::cout << cached_chars.size() << std::endl;

//        std::set<char16_t> diff = c.get_char_diff(all_char_list);

//        std::cout << diff.size() << std::endl;

//        // diff should return a set of one element which contains the newly added char
//        CHECK(diff.size() == 1);
//        CHECK(diff.contains(28779));

//    }






//    SECTION("PBF parsing")
//    {
//        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "vectortile.mvt");
//        QFile file(filepath);
//        file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
//        QByteArray data = file.readAll();

//        CHECK(data.size() > 0);
//        const auto id = tile::Id { .zoom_level = 13, .coords = { 4384, 2878 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

//        const auto vectortile = nucleus::vectortile::VectorTileManager::to_vector_tile(id, data, nullptr);

//        REQUIRE(vectortile->contains(nucleus::vectortile::FeatureType::Peak));

//        // std::cout << vectortile.at(nucleus::vectortile::FeatureType::Peak).size() << std::endl;
//        CHECK(vectortile->at(nucleus::vectortile::FeatureType::Peak).size() == 16);

//        for (const auto& peak : vectortile->at(nucleus::vectortile::FeatureType::Peak)) {

//            // std::cout << peak->labelText().toStdString() << " " << peak->id << std::endl;
//            // std::cout << peak->position.x << ", " << peak->position.y << std::endl;

//            if (peak->id == 26863041ul) {
//                // Check if großglockner has been successfully parsed (note the id might change)
//                CHECK(peak->name == "Großglockner");

//                CHECK(peak->labelText().toStdU16String() == u"Großglockner (3798m)");
//            }
//        }
//    }

//    SECTION("Tile download")
//    {
//        const auto id = tile::Id { .zoom_level = 13, .coords = { 4384, 2878 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

//        nucleus::tile_scheduler::TileLoadService service(
//            nucleus::vectortile::VectorTileManager::TILE_SERVER, nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, ".mvt");
//        // std::cout << "loading: " << qUtf8Printable(service.build_tile_url(id)) << std::endl;
//        {
//            QSignalSpy spy(&service, &nucleus::tile_scheduler::TileLoadService::load_finished);
//            service.load(id);
//            spy.wait(10000);

//            REQUIRE(spy.count() == 1);
//            QList<QVariant> arguments = spy.takeFirst();
//            REQUIRE(arguments.size() == 1);
//            nucleus::tile_scheduler::tile_types::TileLayer tile = arguments.at(0).value<nucleus::tile_scheduler::tile_types::TileLayer>();
//            CHECK(tile.id == id);
//            CHECK(tile.network_info.status == nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good);
//            CHECK(nucleus::tile_scheduler::utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

//            QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "vectortile.mvt");
//            QFile file(filepath);
//            file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
//            QByteArray data = file.readAll();

//            REQUIRE(tile.data->size() > 0);
//            CHECK(tile.data->size() == data.size());
//            CHECK(*tile.data == data);
//        }
//    }

//    SECTION("Tile download parent")
//    {
//        nucleus::tile_scheduler::TileLoadService service(
//            nucleus::vectortile::VectorTileManager::TILE_SERVER, nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, ".mvt");

//        // tile that contains großglockner
//        auto id = tile::Id { .zoom_level = 14, .coords = { 8769, 5757 }, .scheme = tile::Scheme::SlippyMap }.to(tile::Scheme::Tms);

//        std::unordered_map<int, size_t> peaks_per_zoom = { { 14, 10 }, { 13, 16 }, { 12, 32 }, { 11, 35 }, { 10, 35 }, { 9, 36 } };

//        for (int i = 14; i > 8; i--) {
//            // std::cout << "loading: " << i << ": " << qUtf8Printable(service.build_tile_url(id)) << std::endl;

//            QSignalSpy spy(&service, &nucleus::tile_scheduler::TileLoadService::load_finished);
//            service.load(id);
//            spy.wait(10000);

//            REQUIRE(spy.count() == 1);
//            QList<QVariant> arguments = spy.takeFirst();
//            REQUIRE(arguments.size() == 1);
//            const auto tile = arguments.at(0).value<nucleus::tile_scheduler::tile_types::TileLayer>();
//            CHECK(tile.id == id);
//            CHECK(tile.network_info.status == nucleus::tile_scheduler::tile_types::NetworkInfo::Status::Good);
//            CHECK(nucleus::tile_scheduler::utils::time_since_epoch() - tile.network_info.timestamp < 10'000);

//            REQUIRE(tile.data->size() > 0);

//            const auto vectortile = nucleus::vectortile::VectorTileManager::to_vector_tile(id, *tile.data, nullptr);

//            REQUIRE(vectortile->contains(nucleus::vectortile::FeatureType::Peak));

//            // std::cout << vectortile.at(nucleus::vectortile::FeatureType::Peak).size() << std::endl;

//            CHECK(vectortile->at(nucleus::vectortile::FeatureType::Peak).size() == peaks_per_zoom.at(tile.id.zoom_level));

//            bool contains_desired_peak = false;

//            for (const auto& peak : vectortile->at(nucleus::vectortile::FeatureType::Peak)) {

//                // std::cout << peak->labelText().toStdString() << " " << peak->id << std::endl;
//                // std::cout << peak->position.x << ", " << peak->position.y << std::endl;

//                if (peak->id == 26863041ul) {
//                    contains_desired_peak = true;
//                    // Check if großglockner has been successfully parsed (note the id might change)
//                    CHECK(peak->name == "Großglockner");

//                    CHECK(peak->labelText().toStdU16String() == u"Großglockner (3798m)");
//                }
//            }

//            CHECK(contains_desired_peak);

//            id = id.parent();
//        }
//    }
}

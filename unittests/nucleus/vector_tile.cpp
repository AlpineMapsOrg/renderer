/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Lucas Dworschak
 * Copyright (C) 2024 Adam Celarek
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

#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <nucleus/tile_scheduler/TileLoadService.h>
#include <nucleus/tile_scheduler/utils.h>
#include <nucleus/vector_tile/parse.h>
#include <radix/tile.h>

TEST_CASE("nucleus/vector_tiles")
{

    SECTION("Tile download")
    {
        // if this fails it is very likely that something on the vector tile server changed
        // manually download the tile from the below link and check if the changes are valid and replace vectortile.mvt with this new file
        // https://osm.cg.tuwien.ac.at/vector_tiles/poi_v1/10/548/359

        const auto id = tile::Id { .zoom_level = 10, .coords = { 548, 359 }, .scheme = tile::Scheme::SlippyMap };

        nucleus::tile_scheduler::TileLoadService service(
            "https://osm.cg.tuwien.ac.at/vector_tiles/poi_v1/", nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, "");

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

            REQUIRE(tile.data->size() > 0);
            CHECK(tile.data->size() > 2000);
        }
    }

    SECTION("Tile parsing")
    {
        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "vectortile.mvt");
        QFile file(filepath);
        file.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QByteArray data = file.readAll();

        CHECK(data.size() > 0);

        const auto vectortile = nucleus::vector_tile::parse::points_of_interest(data, nullptr);

        CHECK(vectortile.size() == 16);

        // all ids present in this tile
        auto all_ids = std::unordered_set<uint64_t> { 9569407690ul, 240050842ul, 26863165ul, 494054611ul, 3600299561ul, 1123125641ul, 240056984ul, 9084394015ul,
            1828616246ul, 10761456533ul, 21700104ul, 494054604ul, 7731531071ul, 7156956658ul, 26863041ul, 9569407683ul };

        CAPTURE(all_ids);
        for (const auto& poi : vectortile) {
            const auto osm_id = poi.attributes["id"].toULongLong();
            CAPTURE(osm_id);
            CHECK(all_ids.contains(osm_id));
            all_ids.erase(osm_id);

            // qDebug() << poi.name << " (" << poi.id << "): " << poi.attributes;

            if (osm_id == 26863041ul) {
                CHECK(poi.name == "Großglockner");
                CHECK(poi.type == nucleus::vector_tile::PointOfInterest::Type::Peak);
                CHECK(poi.attributes["prominence"] == "2428");
            }
            if (osm_id == 10761456533ul) {
                CHECK(poi.name == "Rojacher Hütte");
                CHECK(poi.type == nucleus::vector_tile::PointOfInterest::Type::AlpineHut);
                CHECK(poi.attributes["operator"] == "Sektion Rauris");
            }
            if (osm_id == 7156956658ul) {
                CHECK(poi.name == "Webcam Gamskopf");
                CHECK(poi.type == nucleus::vector_tile::PointOfInterest::Type::Webcam);
                CHECK(poi.attributes["description"] == "Blickrichtung Norden über Rauris");
            }
            if (osm_id == 21700104ul) {
                CHECK(poi.name == "Kaprun");
                CHECK(poi.type == nucleus::vector_tile::PointOfInterest::Type::Settlement);
                CHECK(poi.attributes["wikidata"] == "Q660671");
            }
        }

        CHECK(all_ids.size() == 0);
    }
}

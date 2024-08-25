/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include "nucleus/tile_scheduler/TileLoadService.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/vector_tiles/VectorTileFeature.h"
#include "nucleus/vector_tiles/VectorTileManager.h"
#include "radix/tile.h"

#include <algorithm>
#include <set>


TEST_CASE("nucleus/vector_tiles")
{

    SECTION("Tile download")
    {
        // https://osm.cg.tuwien.ac.at/vector_tiles/poi_v1/10/548/359

        const auto id = tile::Id { .zoom_level = 10, .coords = { 548, 359 }, .scheme = tile::Scheme::SlippyMap };

        nucleus::tile_scheduler::TileLoadService service(
            nucleus::vectortile::VectorTileManager::tile_server, nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, "");

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

        const auto vectortile = nucleus::vectortile::VectorTileManager::to_vector_tile(data, nullptr);

        CHECK(vectortile->size() == 16);

        // all ids present in this tile
        auto all_ids = std::unordered_set<uint64_t> { 9569407690ul, 7156956658ul, 26863041ul, 26863165ul, 21700104ul, 494054611ul, 240050842ul, 494054604ul,
            240056984ul, 9084394015ul, 7731531071ul, 3600299561ul, 1828616246ul, 10761456533ul, 1123125641ul, 9569407683ul };

        CAPTURE(all_ids);
        for (const auto& poi : *vectortile) {
            CAPTURE(poi);
            CHECK(all_ids.contains(poi->id));
            all_ids.erase(poi->id);

            if (poi->id == 26863041ul) {
                // Check if großglockner has been successfully parsed (note the id might change)
                CHECK(poi->name == "Großglockner");

                CHECK(poi->labelText().toStdU16String() == u"Großglockner (3798m)");
            }
        }

        CHECK(all_ids.size() == 0);
    }
}

/*****************************************************************************
 * AlpineMaps.org
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

#include "nucleus/tile/QuadAssembler.h"

#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>

using namespace nucleus::tile;

namespace {
Data good_tile(const Id& id, const char* bytes) { return { id, { NetworkInfo::Status::Good, nucleus::utils::time_since_epoch() }, std::make_shared<QByteArray>(bytes) }; }
Data missing_tile(const Id& id) { return { id, { NetworkInfo::Status::NotFound, nucleus::utils::time_since_epoch() }, std::make_shared<QByteArray>() }; }
}

TEST_CASE("nucleus/tile/quad assembler")
{
    QuadAssembler assembler;
    SECTION("request children")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_requested(&assembler, &QuadAssembler::tile_requested);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(Id { 0, { 0, 0 } });
        REQUIRE(spy_requested.size() == 4);
        CHECK(spy_requested[0].constFirst().value<Id>() == Id { 1, { 0, 0 } }); // order should not matter.
        CHECK(spy_requested[1].constFirst().value<Id>() == Id { 1, { 1, 0 } });
        CHECK(spy_requested[2].constFirst().value<Id>() == Id { 1, { 0, 1 } });
        CHECK(spy_requested[3].constFirst().value<Id>() == Id { 1, { 1, 1 } });
        CHECK(spy_loaded.empty());
    }

    SECTION("assemble 1")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(Id { 0, { 0, 0 } });

        assembler.deliver_tile(good_tile({ 1, { 0, 0 } }, "dta 100"));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(good_tile({ 1, { 0, 1 } }, "dta 101"));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(good_tile({ 1, { 1, 0 } }, "dta 110"));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(good_tile({ 1, { 1, 1 } }, "dta 111"));
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 0);

        auto loaded_tile = spy_loaded.constFirst().constFirst().value<DataQuad>();
        CHECK(loaded_tile.id == Id { 0, { 0, 0 } });
        CHECK(loaded_tile.network_info().status == NetworkInfo::Status::Good);
        REQUIRE(loaded_tile.n_tiles == 4);
        CHECK(loaded_tile.tiles[0].id == Id { 1, { 0, 0 } }); // order should not matter
        CHECK(loaded_tile.tiles[1].id == Id { 1, { 0, 1 } });
        CHECK(loaded_tile.tiles[2].id == Id { 1, { 1, 0 } });
        CHECK(loaded_tile.tiles[3].id == Id { 1, { 1, 1 } });
        for (unsigned i = 0; i < 4; ++i) {
            REQUIRE(loaded_tile.tiles[i].data);

            const auto tile = loaded_tile.tiles[i];
            const auto number = std::to_string(tile.id.zoom_level) + std::to_string(tile.id.coords.x) + std::to_string(tile.id.coords.y);
            CHECK(*tile.data == QByteArray((std::string("dta ") + number).c_str()));
        }
    }

    SECTION("assemble 2")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(Id { 3, { 4, 5 } }); // happens to be europe ^^

        assembler.deliver_tile(good_tile({ 4, { 8, 10 } }, "ortho 4810"));
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(good_tile({ 4, { 8, 11 } }, "ortho 4811"));
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(good_tile({ 4, { 9, 11 } }, "ortho 4911"));
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(good_tile({ 4, { 9, 10 } }, "ortho 4910"));
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 0);

        auto loaded_tile = spy_loaded.constFirst().constFirst().value<DataQuad>();
        CHECK(loaded_tile.id == Id { 3, { 4, 5 } });
        REQUIRE(loaded_tile.n_tiles == 4);
        CHECK(loaded_tile.tiles[0].id == Id { 4, { 8, 10 } }); // order should not matter
        CHECK(loaded_tile.tiles[1].id == Id { 4, { 8, 11 } });
        CHECK(loaded_tile.tiles[2].id == Id { 4, { 9, 11 } });
        CHECK(loaded_tile.tiles[3].id == Id { 4, { 9, 10 } });
        for (unsigned i = 0; i < 4; ++i) {
            REQUIRE(loaded_tile.tiles[i].data);

            const auto tile = loaded_tile.tiles[i];
            const auto number = std::to_string(tile.id.zoom_level) + std::to_string(tile.id.coords.x) + std::to_string(tile.id.coords.y);
            CHECK(*tile.data == QByteArray((std::string("ortho ") + number).c_str()));
        }
    }

    SECTION("assemble 3 (several tiles in flight)")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(Id { 0, { 0, 0 } });
        assembler.load(Id { 3, { 4, 5 } }); // happens to be europe ^^

        assembler.deliver_tile(good_tile({ 4, { 8, 10 } }, "ortho 4810"));
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(good_tile({ 1, { 0, 1 } }, "ortho 101"));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 2);

        assembler.deliver_tile(good_tile({ 4, { 8, 11 } }, "ortho 4811"));
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(good_tile({ 1, { 1, 0 } }, "ortho 110"));
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(good_tile({ 4, { 9, 11 } }, "ortho 4911"));
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(good_tile({ 4, { 9, 10 } }, "ortho 4910"));
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(good_tile({ 1, { 0, 0 } }, "ortho 100"));
        CHECK(spy_loaded.size() == 1);

        assembler.deliver_tile(good_tile({ 1, { 1, 1 } }, "ortho 111"));
        CHECK(spy_loaded.size() == 2);
        CHECK(assembler.n_items_in_flight() == 0);

        {
            auto loaded_tile = spy_loaded[0].constFirst().value<DataQuad>();
            CHECK(loaded_tile.id == Id { 3, { 4, 5 } });
            REQUIRE(loaded_tile.n_tiles == 4);
            CHECK(loaded_tile.tiles[0].id == Id { 4, { 8, 10 } }); // order should not matter
            CHECK(loaded_tile.tiles[1].id == Id { 4, { 8, 11 } });
            CHECK(loaded_tile.tiles[2].id == Id { 4, { 9, 11 } });
            CHECK(loaded_tile.tiles[3].id == Id { 4, { 9, 10 } });
            for (unsigned i = 0; i < 4; ++i) {
                REQUIRE(loaded_tile.tiles[i].data);

                const auto tile = loaded_tile.tiles[i];
                const auto number = std::to_string(tile.id.zoom_level) + std::to_string(tile.id.coords.x) + std::to_string(tile.id.coords.y);
                CHECK(*tile.data == QByteArray((std::string("ortho ") + number).c_str()));
            }
        }

        {
            auto loaded_tile = spy_loaded[1].constFirst().value<DataQuad>();
            CHECK(loaded_tile.id == Id { 0, { 0, 0 } });
            REQUIRE(loaded_tile.n_tiles == 4);
            CHECK(loaded_tile.tiles[0].id == Id { 1, { 0, 1 } }); // order should not matter
            CHECK(loaded_tile.tiles[1].id == Id { 1, { 1, 0 } });
            CHECK(loaded_tile.tiles[2].id == Id { 1, { 0, 0 } });
            CHECK(loaded_tile.tiles[3].id == Id { 1, { 1, 1 } });
            for (unsigned i = 0; i < 4; ++i) {
                REQUIRE(loaded_tile.tiles[i].data);

                const auto tile = loaded_tile.tiles[i];
                const auto number = std::to_string(tile.id.zoom_level) + std::to_string(tile.id.coords.x) + std::to_string(tile.id.coords.y);
                CHECK(*tile.data == QByteArray((std::string("ortho ") + number).c_str()));
            }
        }
    }


    SECTION("assemble 4 (including missing tiles)")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(Id { 0, { 0, 0 } });

        assembler.deliver_tile(good_tile({ 1, { 0, 0 } }, "ortho 100"));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(missing_tile({ 1, { 0, 1 } }));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(good_tile({ 1, { 1, 0 } }, "ortho 110"));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(good_tile({ 1, { 1, 1 } }, "ortho 111"));
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 0);

        auto loaded_tile = spy_loaded.constFirst().constFirst().value<DataQuad>();
        CHECK(loaded_tile.id == Id { 0, { 0, 0 } });
        CHECK(loaded_tile.network_info().status == NetworkInfo::Status::NotFound);
    }
}

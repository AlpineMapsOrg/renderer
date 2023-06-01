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

#include "nucleus/tile_scheduler/QuadAssembler.h"

#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>

using namespace nucleus::tile_scheduler;

TEST_CASE("nucleus/tile_scheduler/quad assembler")
{
    QuadAssembler assembler;
    SECTION("request children")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_requested(&assembler, &QuadAssembler::tile_requested);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(tile::Id { 0, { 0, 0 } });
        REQUIRE(spy_requested.size() == 4);
        CHECK(spy_requested[0].constFirst().value<tile::Id>() == tile::Id { 1, { 0, 0 } }); // order should not matter.
        CHECK(spy_requested[1].constFirst().value<tile::Id>() == tile::Id { 1, { 1, 0 } });
        CHECK(spy_requested[2].constFirst().value<tile::Id>() == tile::Id { 1, { 0, 1 } });
        CHECK(spy_requested[3].constFirst().value<tile::Id>() == tile::Id { 1, { 1, 1 } });
        CHECK(spy_loaded.empty());
    }

    SECTION("assemble 1")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(tile::Id { 0, { 0, 0 } });

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 1, { 0, 0 } }, std::make_shared<QByteArray>("ortho 100"), std::make_shared<QByteArray>("height 100") });
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 1, { 0, 1 } }, std::make_shared<QByteArray>("ortho 101"), std::make_shared<QByteArray>("height 101") });
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 1, { 1, 0 } }, std::make_shared<QByteArray>("ortho 110"), std::make_shared<QByteArray>("height 110") });
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 1, { 1, 1 } }, std::make_shared<QByteArray>("ortho 111"), std::make_shared<QByteArray>("height 111") });
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 0);

        auto loaded_tile = spy_loaded.constFirst().constFirst().value<tile_types::TileQuad>();
        CHECK(loaded_tile.id == tile::Id { 0, { 0, 0 } });
        REQUIRE(loaded_tile.n_tiles == 4);
        CHECK(loaded_tile.tiles[0].id == tile::Id { 1, { 0, 0 } }); // order should not matter
        CHECK(loaded_tile.tiles[1].id == tile::Id { 1, { 0, 1 } });
        CHECK(loaded_tile.tiles[2].id == tile::Id { 1, { 1, 0 } });
        CHECK(loaded_tile.tiles[3].id == tile::Id { 1, { 1, 1 } });
        for (unsigned i = 0; i < 4; ++i) {
            REQUIRE(loaded_tile.tiles[i].height);
            REQUIRE(loaded_tile.tiles[i].ortho);

            const auto tile = loaded_tile.tiles[i];
            const auto number = std::to_string(tile.id.zoom_level) + std::to_string(tile.id.coords.x) + std::to_string(tile.id.coords.y);
            CHECK(*tile.height == QByteArray((std::string("height ") + number).c_str()));
            CHECK(*tile.ortho == QByteArray((std::string("ortho ") + number).c_str()));
        }
    }

    SECTION("assemble 2")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(tile::Id { 3, { 4, 5 } }); // happens to be europe ^^

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 4, { 8, 10 } }, std::make_shared<QByteArray>("ortho 4810"), std::make_shared<QByteArray>("height 4810") });
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 4, { 8, 11 } }, std::make_shared<QByteArray>("ortho 4811"), std::make_shared<QByteArray>("height 4811") });
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 4, { 9, 11 } }, std::make_shared<QByteArray>("ortho 4911"), std::make_shared<QByteArray>("height 4911") });
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 4, { 9, 10 } }, std::make_shared<QByteArray>("ortho 4910"), std::make_shared<QByteArray>("height 4910") });
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 0);

        auto loaded_tile = spy_loaded.constFirst().constFirst().value<tile_types::TileQuad>();
        CHECK(loaded_tile.id == tile::Id { 3, { 4, 5 } });
        REQUIRE(loaded_tile.n_tiles == 4);
        CHECK(loaded_tile.tiles[0].id == tile::Id { 4, { 8, 10 } }); // order should not matter
        CHECK(loaded_tile.tiles[1].id == tile::Id { 4, { 8, 11 } });
        CHECK(loaded_tile.tiles[2].id == tile::Id { 4, { 9, 11 } });
        CHECK(loaded_tile.tiles[3].id == tile::Id { 4, { 9, 10 } });
        for (unsigned i = 0; i < 4; ++i) {
            REQUIRE(loaded_tile.tiles[i].height);
            REQUIRE(loaded_tile.tiles[i].ortho);

            const auto tile = loaded_tile.tiles[i];
            const auto number = std::to_string(tile.id.zoom_level) + std::to_string(tile.id.coords.x) + std::to_string(tile.id.coords.y);
            CHECK(*tile.height == QByteArray((std::string("height ") + number).c_str()));
            CHECK(*tile.ortho == QByteArray((std::string("ortho ") + number).c_str()));
        }
    }

    SECTION("assemble 3 (several tiles in flight)")
    {
        CHECK(assembler.n_items_in_flight() == 0);
        QSignalSpy spy_loaded(&assembler, &QuadAssembler::quad_loaded);

        assembler.load(tile::Id { 0, { 0, 0 } });
        assembler.load(tile::Id { 3, { 4, 5 } }); // happens to be europe ^^

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 4, { 8, 10 } }, std::make_shared<QByteArray>("ortho 4810"), std::make_shared<QByteArray>("height 4810") });
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 1, { 0, 1 } }, std::make_shared<QByteArray>("ortho 101"), std::make_shared<QByteArray>("height 101") });
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 2);

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 4, { 8, 11 } }, std::make_shared<QByteArray>("ortho 4811"), std::make_shared<QByteArray>("height 4811") });
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 1, { 1, 0 } }, std::make_shared<QByteArray>("ortho 110"), std::make_shared<QByteArray>("height 110") });
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 4, { 9, 11 } }, std::make_shared<QByteArray>("ortho 4911"), std::make_shared<QByteArray>("height 4911") });
        CHECK(spy_loaded.empty());

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 4, { 9, 10 } }, std::make_shared<QByteArray>("ortho 4910"), std::make_shared<QByteArray>("height 4910") });
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 1, { 0, 0 } }, std::make_shared<QByteArray>("ortho 100"), std::make_shared<QByteArray>("height 100") });
        CHECK(spy_loaded.size() == 1);

        assembler.deliver_tile(tile_types::LayeredTile { tile::Id { 1, { 1, 1 } }, std::make_shared<QByteArray>("ortho 111"), std::make_shared<QByteArray>("height 111") });
        CHECK(spy_loaded.size() == 2);
        CHECK(assembler.n_items_in_flight() == 0);

        {
            auto loaded_tile = spy_loaded[0].constFirst().value<tile_types::TileQuad>();
            CHECK(loaded_tile.id == tile::Id { 3, { 4, 5 } });
            REQUIRE(loaded_tile.n_tiles == 4);
            CHECK(loaded_tile.tiles[0].id == tile::Id { 4, { 8, 10 } }); // order should not matter
            CHECK(loaded_tile.tiles[1].id == tile::Id { 4, { 8, 11 } });
            CHECK(loaded_tile.tiles[2].id == tile::Id { 4, { 9, 11 } });
            CHECK(loaded_tile.tiles[3].id == tile::Id { 4, { 9, 10 } });
            for (unsigned i = 0; i < 4; ++i) {
                REQUIRE(loaded_tile.tiles[i].height);
                REQUIRE(loaded_tile.tiles[i].ortho);

                const auto tile = loaded_tile.tiles[i];
                const auto number = std::to_string(tile.id.zoom_level) + std::to_string(tile.id.coords.x) + std::to_string(tile.id.coords.y);
                CHECK(*tile.height == QByteArray((std::string("height ") + number).c_str()));
                CHECK(*tile.ortho == QByteArray((std::string("ortho ") + number).c_str()));
            }
        }

        {
            auto loaded_tile = spy_loaded[1].constFirst().value<tile_types::TileQuad>();
            CHECK(loaded_tile.id == tile::Id { 0, { 0, 0 } });
            REQUIRE(loaded_tile.n_tiles == 4);
            CHECK(loaded_tile.tiles[0].id == tile::Id { 1, { 0, 1 } }); // order should not matter
            CHECK(loaded_tile.tiles[1].id == tile::Id { 1, { 1, 0 } });
            CHECK(loaded_tile.tiles[2].id == tile::Id { 1, { 0, 0 } });
            CHECK(loaded_tile.tiles[3].id == tile::Id { 1, { 1, 1 } });
            for (unsigned i = 0; i < 4; ++i) {
                REQUIRE(loaded_tile.tiles[i].height);
                REQUIRE(loaded_tile.tiles[i].ortho);

                const auto tile = loaded_tile.tiles[i];
                const auto number = std::to_string(tile.id.zoom_level) + std::to_string(tile.id.coords.x) + std::to_string(tile.id.coords.y);
                CHECK(*tile.height == QByteArray((std::string("height ") + number).c_str()));
                CHECK(*tile.ortho == QByteArray((std::string("ortho ") + number).c_str()));
            }
        }
    }
}

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

#include "nucleus/tile_scheduler/LayerAssembler.h"

#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>

using nucleus::tile_scheduler::LayerAssembler;
using nucleus::tile_scheduler::tile_types::LayeredTile;

TEST_CASE("nucleus/tile_scheduler/layer assembler")
{
    LayerAssembler assembler;
    SECTION("request only once")
    {
        QSignalSpy spy_requested(&assembler, &LayerAssembler::tile_requested);
        QSignalSpy spy_loaded(&assembler, &LayerAssembler::tile_loaded);

        assembler.load(tile::Id { 0, { 0, 0 } });
        REQUIRE(spy_requested.size() == 1);
        CHECK(spy_requested.constFirst().constFirst().value<tile::Id>() == tile::Id { 0, { 0, 0 } });
        CHECK(spy_loaded.empty());
    }

    SECTION("assemble 1 (ortho, height)")
    {
        QSignalSpy spy_requested(&assembler, &LayerAssembler::tile_requested);
        QSignalSpy spy_loaded(&assembler, &LayerAssembler::tile_loaded);

        assembler.load(tile::Id { 0, { 0, 0 } });

        assembler.deliver_ortho(tile::Id { 0, { 0, 0 } }, std::make_shared<const QByteArray>("ortho"));
        CHECK(spy_requested.size() == 1);
        CHECK(spy_loaded.empty());

        assembler.deliver_height(tile::Id { 0, { 0, 0 } }, std::make_shared<const QByteArray>("height"));

        CHECK(spy_requested.size() == 1);
        REQUIRE(spy_loaded.size() == 1);
        auto loaded_tile = spy_loaded.constFirst().constFirst().value<LayeredTile>();
        CHECK(loaded_tile.id == tile::Id { 0, { 0, 0 } });
        REQUIRE(loaded_tile.ortho);
        REQUIRE(loaded_tile.height);
        CHECK(*loaded_tile.ortho == QByteArray("ortho"));
        CHECK(*loaded_tile.height == QByteArray("height"));
        CHECK(assembler.n_items_in_flight() == 0);
    }

    SECTION("assemble 2 (height, ortho)")
    {
        QSignalSpy spy_requested(&assembler, &LayerAssembler::tile_requested);
        QSignalSpy spy_loaded(&assembler, &LayerAssembler::tile_loaded);
        assembler.load(tile::Id { 0, { 0, 0 } });

        assembler.deliver_height(tile::Id { 0, { 0, 0 } }, std::make_shared<const QByteArray>("height"));
        CHECK(spy_requested.size() == 1);
        CHECK(spy_loaded.empty());

        assembler.deliver_ortho(tile::Id { 0, { 0, 0 } }, std::make_shared<const QByteArray>("ortho"));

        CHECK(spy_requested.size() == 1);
        REQUIRE(spy_loaded.size() == 1);
        auto loaded_tile = spy_loaded.constFirst().constFirst().value<LayeredTile>();
        CHECK(loaded_tile.id == tile::Id { 0, { 0, 0 } });
        REQUIRE(loaded_tile.ortho);
        REQUIRE(loaded_tile.height);
        CHECK(*loaded_tile.ortho == QByteArray("ortho"));
        CHECK(*loaded_tile.height == QByteArray("height"));
        CHECK(assembler.n_items_in_flight() == 0);
    }

    SECTION("assemble 3 (several tiles)")
    {
        QSignalSpy spy_requested(&assembler, &LayerAssembler::tile_requested);
        QSignalSpy spy_loaded(&assembler, &LayerAssembler::tile_loaded);
        assembler.load(tile::Id { 0, { 0, 0 } });
        assembler.load(tile::Id { 1, { 0, 0 } });
        assembler.load(tile::Id { 2, { 0, 0 } });
        REQUIRE(spy_requested.size() == 3);
        for (int i = 0; i < spy_requested.size(); ++i) {
            CHECK(spy_requested.at(i).constFirst().value<tile::Id>() == tile::Id { unsigned(i), { 0, 0 } });
        }

        assembler.deliver_height(tile::Id { 0, { 0, 0 } }, std::make_shared<const QByteArray>("height 0"));
        assembler.deliver_ortho(tile::Id { 1, { 0, 0 } }, std::make_shared<const QByteArray>("ortho 1"));
        assembler.deliver_height(tile::Id { 2, { 0, 0 } }, std::make_shared<const QByteArray>("height 2"));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 3);

        assembler.deliver_ortho(tile::Id { 2, { 0, 0 } }, std::make_shared<const QByteArray>("ortho 2"));
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 2);

        assembler.deliver_ortho(tile::Id { 0, { 0, 0 } }, std::make_shared<const QByteArray>("ortho 0"));
        CHECK(spy_loaded.size() == 2);
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_height(tile::Id { 1, { 0, 0 } }, std::make_shared<const QByteArray>("height 1"));
        CHECK(spy_loaded.size() == 3);
        CHECK(assembler.n_items_in_flight() == 0);

        REQUIRE(spy_loaded.size() == 3);
        CHECK(spy_loaded.at(0).constFirst().value<LayeredTile>().id == tile::Id { 2, { 0, 0 } });
        CHECK(spy_loaded.at(1).constFirst().value<LayeredTile>().id == tile::Id { 0, { 0, 0 } });
        CHECK(spy_loaded.at(2).constFirst().value<LayeredTile>().id == tile::Id { 1, { 0, 0 } });

        for (int i = 0; i < spy_loaded.size(); ++i) {
            const auto tile = spy_loaded.at(0).constFirst().value<LayeredTile>();
            CHECK(*tile.height == QByteArray((std::string("height ") + std::to_string(tile.id.zoom_level)).c_str()));
            CHECK(*tile.ortho == QByteArray((std::string("ortho ") + std::to_string(tile.id.zoom_level)).c_str()));
        }
    }

    SECTION("a layer (ortho) reported missing")
    {
        QSignalSpy spy_loaded(&assembler, &LayerAssembler::tile_loaded);
        assembler.load(tile::Id { 0, { 0, 0 } });

        assembler.deliver_height(tile::Id { 0, { 0, 0 } }, std::make_shared<const QByteArray>("height"));
        assembler.report_missing_ortho(tile::Id { 0, { 0, 0 } });

        REQUIRE(spy_loaded.size() == 1);
        auto loaded_tile = spy_loaded.constFirst().constFirst().value<LayeredTile>();
        CHECK(loaded_tile.id == tile::Id { 0, { 0, 0 } });
        REQUIRE(!loaded_tile.ortho);
        REQUIRE(loaded_tile.height);
        CHECK(*loaded_tile.height == QByteArray("height"));
        CHECK(assembler.n_items_in_flight() == 0);
    }

    SECTION("a layer (height) reported missing")
    {
        QSignalSpy spy_loaded(&assembler, &LayerAssembler::tile_loaded);
        assembler.load(tile::Id { 0, { 0, 0 } });

        assembler.deliver_ortho(tile::Id { 0, { 0, 0 } }, std::make_shared<const QByteArray>("orthgo"));
        assembler.report_missing_height(tile::Id { 0, { 0, 0 } });

        REQUIRE(spy_loaded.size() == 1);
        auto loaded_tile = spy_loaded.constFirst().constFirst().value<LayeredTile>();
        CHECK(loaded_tile.id == tile::Id { 0, { 0, 0 } });
        REQUIRE(loaded_tile.ortho);
        REQUIRE(!loaded_tile.height);
        CHECK(*loaded_tile.ortho == QByteArray("orthgo"));
        CHECK(assembler.n_items_in_flight() == 0);
    }
}

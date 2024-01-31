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

#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>

#include "nucleus/tile_scheduler/LayerAssembler.h"
#include "nucleus/tile_scheduler/utils.h"

using namespace nucleus::tile_scheduler;
using namespace tile_types;

namespace {
TileLayer good_tile(const tile::Id& id, const char* bytes) {
    return {id, {NetworkInfo::Status::Good, utils::time_since_epoch()}, std::make_shared<QByteArray>(bytes)};
}
TileLayer missing_tile(const tile::Id& id) {
    return {id, {NetworkInfo::Status::NotFound, utils::time_since_epoch()}, std::make_shared<QByteArray>()};
}
TileLayer network_failed_tile(const tile::Id& id) {
    return {id, {NetworkInfo::Status::NetworkError, utils::time_since_epoch()}, std::make_shared<QByteArray>()};
}
}

TEST_CASE("nucleus/tile_scheduler/layer assembler")
{
    LayerAssembler assembler;
    SECTION("layer joining") {
        {
            const auto joined = LayerAssembler::join(good_tile({ 0, { 0, 0 } }, "ortho"), good_tile({ 0, { 0, 0 } }, "height"));
            CHECK(joined.id == tile::Id{ 0, { 0, 0 } });
            CHECK(joined.network_info.status == NetworkInfo::Status::Good);
            CHECK(*joined.ortho == "ortho");
            CHECK(*joined.height == "height");
        }
        {
            const auto joined = LayerAssembler::join(good_tile({ 0, { 0, 0 } }, "ortho"), missing_tile({ 0, { 0, 0 } }));
            CHECK(joined.id == tile::Id{ 0, { 0, 0 } });
            CHECK(joined.network_info.status == NetworkInfo::Status::NotFound);
            CHECK(joined.ortho->isEmpty());
            CHECK(joined.height->isEmpty());
        }
        {
            const auto joined = LayerAssembler::join(network_failed_tile({ 0, { 0, 0 } }), missing_tile({ 0, { 0, 0 } }));
            CHECK(joined.id == tile::Id{ 0, { 0, 0 } });
            CHECK(joined.network_info.status == NetworkInfo::Status::NetworkError);
            CHECK(joined.ortho->isEmpty());
            CHECK(joined.height->isEmpty());
        }
    }

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

        assembler.deliver_ortho(good_tile({ 0, { 0, 0 } }, "ortho"));
        CHECK(spy_requested.size() == 1);
        CHECK(spy_loaded.empty());

        assembler.deliver_height(good_tile({ 0, { 0, 0 } }, "height"));

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

        assembler.deliver_height(good_tile({ 0, { 0, 0 } }, "height"));
        CHECK(spy_requested.size() == 1);
        CHECK(spy_loaded.empty());

        assembler.deliver_ortho(good_tile({ 0, { 0, 0 } }, "ortho"));

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

        assembler.deliver_height(good_tile({ 0, { 0, 0 } }, "height 0"));
        assembler.deliver_ortho(good_tile({ 1, { 0, 0 } }, "ortho 1"));
        assembler.deliver_height(good_tile({ 2, { 0, 0 } }, "height 2"));
        CHECK(spy_loaded.empty());
        CHECK(assembler.n_items_in_flight() == 3);

        assembler.deliver_ortho(good_tile({ 2, { 0, 0 } }, "ortho 2"));
        CHECK(spy_loaded.size() == 1);
        CHECK(assembler.n_items_in_flight() == 2);

        assembler.deliver_ortho(good_tile({ 0, { 0, 0 } }, "ortho 0"));
        CHECK(spy_loaded.size() == 2);
        CHECK(assembler.n_items_in_flight() == 1);

        assembler.deliver_height(good_tile({ 1, { 0, 0 } }, "height 1"));
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

        assembler.deliver_height(good_tile({ 0, { 0, 0 } }, "height"));
        assembler.deliver_ortho(missing_tile({ 0, { 0, 0 } }));

        REQUIRE(spy_loaded.size() == 1);
        auto loaded_tile = spy_loaded.constFirst().constFirst().value<LayeredTile>();
        CHECK(loaded_tile.id == tile::Id { 0, { 0, 0 } });
        REQUIRE(!loaded_tile.ortho->size());
        REQUIRE(!loaded_tile.height->size());
        CHECK(assembler.n_items_in_flight() == 0);
    }

    SECTION("a layer (height) reported missing")
    {
        QSignalSpy spy_loaded(&assembler, &LayerAssembler::tile_loaded);
        assembler.load(tile::Id { 0, { 0, 0 } });

        assembler.deliver_ortho(good_tile({ 0, { 0, 0 } }, "orthgo"));
        assembler.deliver_height(missing_tile({ 0, { 0, 0 } }));

        REQUIRE(spy_loaded.size() == 1);
        auto loaded_tile = spy_loaded.constFirst().constFirst().value<LayeredTile>();
        CHECK(loaded_tile.id == tile::Id { 0, { 0, 0 } });
        REQUIRE(!loaded_tile.ortho->size());
        REQUIRE(!loaded_tile.height->size());
        CHECK(assembler.n_items_in_flight() == 0);
    }
}

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

#include <catch2/catch.hpp>

#include <sherpa/tile.h>

#include <QSignalSpy>

#include <nucleus/tile_scheduler/tile_types.h>

#include "nucleus/tile_scheduler/SlotLimiter.h"

TEST_CASE("nucleus/tile_scheduler/slot limiter")
{
    using namespace nucleus::tile_scheduler;
    SECTION("doesn't move when requesting empty array")
    {
        SlotLimiter sl;
        QSignalSpy spy(&sl, &SlotLimiter::quad_requested);
        sl.request_quads({});
        CHECK(spy.size() == 0);
    }

    SECTION("sends on requests")
    {
        SlotLimiter sl;
        QSignalSpy spy(&sl, &SlotLimiter::quad_requested);
        sl.request_quads({ tile::Id { 0, { 0, 0 } },
            tile::Id { 1, { 0, 0 } } });
        REQUIRE(spy.size() == 2);
        CHECK(spy[0][0].value<tile::Id>() == tile::Id { 0, { 0, 0 } });
        CHECK(spy[1][0].value<tile::Id>() == tile::Id { 1, { 0, 0 } });
    }

    SECTION("sends on requests only up to the limit of tile slots")
    {
        SlotLimiter sl;
        sl.set_limit(2);
        QSignalSpy spy(&sl, &SlotLimiter::quad_requested);
        sl.request_quads({ tile::Id { 0, { 0, 0 } },
            tile::Id { 1, { 0, 0 } },
            tile::Id { 1, { 0, 1 } } });
        REQUIRE(spy.size() == 2);
        CHECK(spy[0][0].value<tile::Id>() == tile::Id { 0, { 0, 0 } });
        CHECK(spy[1][0].value<tile::Id>() == tile::Id { 1, { 0, 0 } });

        sl.request_quads({ tile::Id { 1, { 1, 0 } } });
        CHECK(spy.size() == 2);
    }

    SECTION("receiving tiles frees up slots")
    {
        SlotLimiter sl;
        sl.set_limit(2);
        QSignalSpy spy(&sl, &SlotLimiter::quad_requested);
        sl.request_quads({ tile::Id { 0, { 0, 0 } },
            tile::Id { 1, { 0, 0 } },
            tile::Id { 1, { 0, 1 } },
            tile::Id { 2, { 0, 0 } } });
        REQUIRE(spy.size() == 2);

        sl.deliver_quad(tile_types::TileQuad { tile::Id { 0, { 0, 0 } } });
        REQUIRE(spy.size() == 3);
        CHECK(spy[2][0].value<tile::Id>() == tile::Id { 1, { 0, 1 } });

        sl.deliver_quad(tile_types::TileQuad { tile::Id { 1, { 0, 0 } } });
        REQUIRE(spy.size() == 4);
        CHECK(spy[3][0].value<tile::Id>() == tile::Id { 2, { 0, 0 } });

        // running out of tile requests
        sl.deliver_quad(tile_types::TileQuad { tile::Id { 1, { 0, 1 } } });
        sl.deliver_quad(tile_types::TileQuad { tile::Id { 2, { 0, 0 } } });
        CHECK(spy.size() == 4);
    }

    SECTION("updating request list omits requesting in flight tiles again")
    {
        SlotLimiter sl;
        sl.set_limit(2);
        QSignalSpy spy(&sl, &SlotLimiter::quad_requested);
        sl.request_quads({ tile::Id { 0, { 0, 0 } },
            tile::Id { 1, { 0, 0 } },
            tile::Id { 2, { 0, 0 } },
            tile::Id { 3, { 0, 0 } } });

        REQUIRE(spy.size() == 2);
        sl.request_quads({ tile::Id { 0, { 0, 0 } }, // already requested
            tile::Id { 1, { 0, 0 } }, // already requested
            tile::Id { 2, { 1, 0 } }, // new, should go next
            tile::Id { 3, { 1, 0 } } }); // new, should go next

        sl.deliver_quad(tile_types::TileQuad { tile::Id { 0, { 0, 0 } } });
        REQUIRE(spy.size() == 3);
        CHECK(spy[2][0].value<tile::Id>() == tile::Id { 2, { 1, 0 } });

        sl.deliver_quad(tile_types::TileQuad { tile::Id { 1, { 0, 0 } } });
        REQUIRE(spy.size() == 4);
        CHECK(spy[3][0].value<tile::Id>() == tile::Id { 3, { 1, 0 } });
    }

    SECTION("delivered quads are sent on")
    {
        SlotLimiter sl;
        QSignalSpy spy(&sl, &SlotLimiter::quads_delivered);
        sl.deliver_quad(tile_types::TileQuad { tile::Id { 0, { 0, 0 } } });
        REQUIRE(spy.size() == 1);
        CHECK(spy[0][0].value<std::vector<tile_types::TileQuad>>()[0].id == tile::Id { 0, { 0, 0 } });

        sl.deliver_quad(tile_types::TileQuad { tile::Id { 1, { 2, 3 } } });
        REQUIRE(spy.size() == 2);
        CHECK(spy[1][0].value<std::vector<tile_types::TileQuad>>()[0].id == tile::Id { 1, { 2, 3 } });
    }
}

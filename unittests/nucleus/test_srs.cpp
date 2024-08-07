/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <unordered_set>

#include <QImage>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <nucleus/camera/PositionStorage.h>
#include <nucleus/srs.h>
#include <nucleus/tile_scheduler/utils.h>
#include <radix/TileHeights.h>
#include <radix/quad_tree.h>

using Catch::Approx;
using namespace nucleus;
using namespace srs;

TEST_CASE("nucleus/srs")
{
    std::unordered_set<tile::Id, tile::Id::Hasher> ids;
    {
        TileHeights h;
        h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
        auto aabb_decorator = tile_scheduler::utils::AabbDecorator::make(std::move(h));

        const auto add_tiles = [&](auto camera) {
            camera.set_viewport_size({ 1920, 1080 });
            const auto all_leaves = quad_tree::onTheFlyTraverse(
                tile::Id { 0, { 0, 0 } }, tile_scheduler::utils::refineFunctor(camera, aabb_decorator, 1, 64), [&ids](const tile::Id& v) {
                    ids.insert(v);
                    return v.children();
                });
        };
        add_tiles(camera::stored_positions::stephansdom());
        add_tiles(camera::stored_positions::grossglockner());
        add_tiles(camera::stored_positions::oestl_hochgrubach_spitze());
        add_tiles(camera::stored_positions::wien());
        add_tiles(camera::stored_positions::karwendel());
        add_tiles(camera::stored_positions::schneeberg());
        add_tiles(camera::stored_positions::weichtalhaus());
        add_tiles(camera::stored_positions::grossglockner_shadow());
    }

    SECTION("number of tiles per level")
    {
        CHECK(number_of_horizontal_tiles_for_zoom_level(0) == 1);
        CHECK(number_of_horizontal_tiles_for_zoom_level(1) == 2);
        CHECK(number_of_horizontal_tiles_for_zoom_level(4) == 16);
        CHECK(number_of_vertical_tiles_for_zoom_level(0) == 1);
        CHECK(number_of_vertical_tiles_for_zoom_level(1) == 2);
        CHECK(number_of_vertical_tiles_for_zoom_level(4) == 16);
    }

    SECTION("tile bounds")
    {
        // https://gis.stackexchange.com/questions/144471/spherical-mercator-world-bounds
        // ACHTUNG: https://epsg.io/3857 shows wrong bounds (as of February 2022).
        constexpr auto b = 20037508.3427892;
        {
            const auto bounds = tile_bounds({ 0, { 0, 0 } });
            CHECK(bounds.min.x == Approx(-b));
            CHECK(bounds.min.y == Approx(-b));
            CHECK(bounds.max.x == Approx(b));
            CHECK(bounds.max.y == Approx(b));
        }
        {
            // we default to TMS mapping, that is, y tile 0 is south
            const auto bounds = tile_bounds({ 1, { 0, 0 } });
            CHECK(bounds.min.x == Approx(-b));
            CHECK(bounds.min.y == Approx(-b));
            CHECK(bounds.max.x == Approx(0));
            CHECK(bounds.max.y == Approx(0));
        }
        {
            const auto bounds = tile_bounds({ 1, { 0, 1 } });
            CHECK(bounds.min.x == Approx(-b));
            CHECK(bounds.min.y == Approx(0));
            CHECK(bounds.max.x == Approx(0));
            CHECK(bounds.max.y == Approx(b));
        }
        {
            const auto bounds = tile_bounds({ 1, { 1, 0 } });
            CHECK(bounds.min.x == Approx(0));
            CHECK(bounds.min.y == Approx(-b));
            CHECK(bounds.max.x == Approx(b));
            CHECK(bounds.max.y == Approx(0));
        }
        {
            const auto bounds = tile_bounds({ 1, { 1, 1 } });
            CHECK(bounds.min.x == Approx(0));
            CHECK(bounds.min.y == Approx(0));
            CHECK(bounds.max.x == Approx(b));
            CHECK(bounds.max.y == Approx(b));
        }
        {
            // computed using alpine terrain builder
            const auto bounds = tile_bounds({ 16, { 34420, 42241 } });
            CHECK(bounds.min.x == Approx(1010191.76581689));
            CHECK(bounds.min.y == Approx(5792703.751563799));
            CHECK(bounds.max.x == Approx(1010803.262043171));
            CHECK(bounds.max.y == Approx(5793315.24779008));
        }
    }

    SECTION("overlap")
    {
        CHECK(overlap(tile::Id { .zoom_level = 0, .coords = { 0, 0 } }, tile::Id { .zoom_level = 0, .coords = { 0, 0 } }));
        CHECK(!overlap(tile::Id { .zoom_level = 1, .coords = { 0, 0 } }, tile::Id { .zoom_level = 1, .coords = { 0, 1 } }));
        CHECK(overlap(tile::Id { .zoom_level = 0, .coords = { 0, 0 } }, tile::Id { .zoom_level = 1, .coords = { 0, 1 } }));
        CHECK(overlap(tile::Id { .zoom_level = 1, .coords = { 0, 0 } }, tile::Id { .zoom_level = 3, .coords = { 2, 1 } }));
        CHECK(!overlap(tile::Id { .zoom_level = 1, .coords = { 0, 0 } }, tile::Id { .zoom_level = 3, .coords = { 0, 7 } }));
    }

    SECTION("srs conversion")
    {
        CHECK(lat_long_to_world({ 0, 0 }).x == Approx(0).scale(20037508));
        CHECK(lat_long_to_world({ 0, 0 }).y == Approx(0).scale(20037508));

        constexpr double maxLat = 85.05112878;
        constexpr double maxLong = 180;
        CHECK(lat_long_to_world({ maxLat, maxLong }).x == Approx(20037508.342789244));
        CHECK(lat_long_to_world({ maxLat, maxLong }).y == Approx(20037508.342789244));

        // https://epsg.io/transform#s_srs=4326&t_srs=3857&x=16.3726561&y=48.2086939
        constexpr double westl_hochgrubach_spitze_lat = 48.2086939;
        constexpr double westl_hochgrubach_spitze_long = 16.3726561;
        CHECK(lat_long_to_world({ westl_hochgrubach_spitze_lat, westl_hochgrubach_spitze_long }).x == Approx(1822595.7412222677));
        CHECK(lat_long_to_world({ westl_hochgrubach_spitze_lat, westl_hochgrubach_spitze_long }).y == Approx(6141644.553721141));
    }

    SECTION("srs conversion two way")
    {
        {
            constexpr double lat = 48.2086939;
            constexpr double lon = 16.3726561;
            CHECK(world_to_lat_long(lat_long_to_world({ lat, lon })).x == Approx(lat));
            CHECK(world_to_lat_long(lat_long_to_world({ lat, lon })).y == Approx(lon));
        }
        {
            constexpr double lat = 12.565;
            constexpr double lon = -125.54;
            CHECK(world_to_lat_long(lat_long_to_world({ lat, lon })).x == Approx(lat));
            CHECK(world_to_lat_long(lat_long_to_world({ lat, lon })).y == Approx(lon));
        }
        {
            constexpr double lat = -12.565;
            constexpr double lon = -165.54;
            CHECK(world_to_lat_long(lat_long_to_world({ lat, lon })).x == Approx(lat));
            CHECK(world_to_lat_long(lat_long_to_world({ lat, lon })).y == Approx(lon));
        }
        {
            constexpr double lat = -65.565;
            constexpr double lon = 135.54;
            CHECK(world_to_lat_long(lat_long_to_world({ lat, lon })).x == Approx(lat));
            CHECK(world_to_lat_long(lat_long_to_world({ lat, lon })).y == Approx(lon));
        }
    }

    SECTION("srs conversion with height")
    {
        constexpr double pi = 3.141592653589793238462643383279502884197169399375105820974944;
        CHECK(lat_long_alt_to_world({ 0, 0, 10.0 }).x == Approx(0).scale(20037508));
        CHECK(lat_long_alt_to_world({ 0, 0, 10.0 }).y == Approx(0).scale(20037508));
        CHECK(lat_long_alt_to_world({ 0, 0, 10.0 }).z == Approx(10).scale(20037508));

        {
            constexpr double lat = 48.2086939;
            constexpr double lon = 16.3726561;
            constexpr double alt = 100.0;
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).x == Approx(lat_long_to_world({ lat, lon }).x).scale(1000));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).y == Approx(lat_long_to_world({ lat, lon }).y).scale(1000));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).z >= Approx(alt));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).z == Approx(alt / std::abs(std::cos(lat * pi / 180.0))));
        }

        {
            constexpr double lat = 38.2086939;
            constexpr double lon = -116.3726561;
            constexpr double alt = 200.0;
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).x == Approx(lat_long_to_world({ lat, lon }).x).scale(1000));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).y == Approx(lat_long_to_world({ lat, lon }).y).scale(1000));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).z >= Approx(alt));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).z == Approx(alt / std::abs(std::cos(lat * pi / 180.0))));
        }

        {
            constexpr double lat = -70.2086939;
            constexpr double lon = 26.3726561;
            constexpr double alt = 1000.0;
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).x == Approx(lat_long_to_world({ lat, lon }).x).scale(1000));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).y == Approx(lat_long_to_world({ lat, lon }).y).scale(1000));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).z >= Approx(alt));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).z == Approx(alt / std::abs(std::cos(lat * pi / 180.0))));
        }

        {
            constexpr double lat = -60.2086939;
            constexpr double lon = -96.3726561;
            constexpr double alt = 4000.0;
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).x == Approx(lat_long_to_world({ lat, lon }).x).scale(1000));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).y == Approx(lat_long_to_world({ lat, lon }).y).scale(1000));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).z >= Approx(alt));
            CHECK(lat_long_alt_to_world({ lat, lon, alt }).z == Approx(alt / std::abs(std::cos(lat * pi / 180.0))));
        }

        {
            constexpr double lat = -60.2086939;
            constexpr double lon = -96.3726561;
            constexpr double alt = 4000.0;
            const auto real_coords = glm::dvec3 { lat, lon, alt };
            CHECK(world_to_lat_long_alt(lat_long_alt_to_world(real_coords)).x == Approx(real_coords.x).scale(1000));
            CHECK(world_to_lat_long_alt(lat_long_alt_to_world(real_coords)).y == Approx(real_coords.y).scale(1000));
            CHECK(world_to_lat_long_alt(lat_long_alt_to_world(real_coords)).z == Approx(real_coords.z).scale(1000));
        }
    }

    SECTION("check conflict potential")
    {
        QImage data(256, 256, QImage::Format_Grayscale8);
        data.fill(0);
        unsigned conflict_chain_length = 0;
        unsigned n_conflicts = 0;
        // unsigned n_equality_checks = 0;
        for (const auto id : ids) {
            const auto hash = hash_uint16(id);
            auto& bucket = (*(data.bits() + hash));
            n_conflicts += bucket != 0;
            bucket++;
            conflict_chain_length = std::max(conflict_chain_length, unsigned(bucket));
        }
        for (int i = 0; i < 256 * 256; ++i) {
            auto& bucket = (*(data.bits() + i));
            bucket *= 255 / conflict_chain_length;
        }

        // data.save("tile_id_hashing.png");
        // qDebug() << "n_tiles: " << ids.size();
        // qDebug() << "conflict_chain_length: " << conflict_chain_length;
        // qDebug() << "n_conflicts: " << n_conflicts;
        // qDebug() << "n_equality_checks: " << n_equality_checks;
        CHECK(conflict_chain_length <= 3);
        CHECK(n_conflicts <= 1000);
    }

    SECTION("packing roundtrip in c++")
    {
        const auto packing_roundtrip_cpp = [](const tile::Id& id) {
            const auto packed = pack(id);
            const auto unpacked = unpack(packed);
            CHECK(id == unpacked);
        };

        packing_roundtrip_cpp({ 0, { 0, 0 } });
        packing_roundtrip_cpp({ 1, { 0, 0 } });
        packing_roundtrip_cpp({ 1, { 1, 1 } });
        packing_roundtrip_cpp({ 14, { 2673, 12038 } });
        packing_roundtrip_cpp({ 20, { 430489, 100204 } });
        for (const auto id : ids) {
            packing_roundtrip_cpp(id);
        }
    }
}

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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nucleus/srs.h"

using Catch::Approx;
using namespace nucleus;

TEST_CASE("nucleus/srs")
{
    SECTION("number of tiles per level")
    {
        CHECK(srs::number_of_horizontal_tiles_for_zoom_level(0) == 1);
        CHECK(srs::number_of_horizontal_tiles_for_zoom_level(1) == 2);
        CHECK(srs::number_of_horizontal_tiles_for_zoom_level(4) == 16);
        CHECK(srs::number_of_vertical_tiles_for_zoom_level(0) == 1);
        CHECK(srs::number_of_vertical_tiles_for_zoom_level(1) == 2);
        CHECK(srs::number_of_vertical_tiles_for_zoom_level(4) == 16);
    }

    SECTION("tile bounds")
    {
        // https://gis.stackexchange.com/questions/144471/spherical-mercator-world-bounds
        // ACHTUNG: https://epsg.io/3857 shows wrong bounds (as of February 2022).
        constexpr auto b = 20037508.3427892;
        {
            const auto bounds = srs::tile_bounds({ 0, { 0, 0 } });
            CHECK(bounds.min.x == Approx(-b));
            CHECK(bounds.min.y == Approx(-b));
            CHECK(bounds.max.x == Approx(b));
            CHECK(bounds.max.y == Approx(b));
        }
        {
            // we default to TMS mapping, that is, y tile 0 is south
            const auto bounds = srs::tile_bounds({ 1, { 0, 0 } });
            CHECK(bounds.min.x == Approx(-b));
            CHECK(bounds.min.y == Approx(-b));
            CHECK(bounds.max.x == Approx(0));
            CHECK(bounds.max.y == Approx(0));
        }
        {
            const auto bounds = srs::tile_bounds({ 1, { 0, 1 } });
            CHECK(bounds.min.x == Approx(-b));
            CHECK(bounds.min.y == Approx(0));
            CHECK(bounds.max.x == Approx(0));
            CHECK(bounds.max.y == Approx(b));
        }
        {
            const auto bounds = srs::tile_bounds({ 1, { 1, 0 } });
            CHECK(bounds.min.x == Approx(0));
            CHECK(bounds.min.y == Approx(-b));
            CHECK(bounds.max.x == Approx(b));
            CHECK(bounds.max.y == Approx(0));
        }
        {
            const auto bounds = srs::tile_bounds({ 1, { 1, 1 } });
            CHECK(bounds.min.x == Approx(0));
            CHECK(bounds.min.y == Approx(0));
            CHECK(bounds.max.x == Approx(b));
            CHECK(bounds.max.y == Approx(b));
        }
        {
            // computed using alpine terrain builder
            const auto bounds = srs::tile_bounds({ 16, { 34420, 42241 } });
            CHECK(bounds.min.x == Approx(1010191.76581689));
            CHECK(bounds.min.y == Approx(5792703.751563799));
            CHECK(bounds.max.x == Approx(1010803.262043171));
            CHECK(bounds.max.y == Approx(5793315.24779008));
        }
    }

    SECTION("overlap")
    {
        CHECK(srs::overlap(tile::Id { .zoom_level = 0, .coords = { 0, 0 } }, tile::Id { .zoom_level = 0, .coords = { 0, 0 } }));
        CHECK(!srs::overlap(tile::Id { .zoom_level = 1, .coords = { 0, 0 } }, tile::Id { .zoom_level = 1, .coords = { 0, 1 } }));
        CHECK(srs::overlap(tile::Id { .zoom_level = 0, .coords = { 0, 0 } }, tile::Id { .zoom_level = 1, .coords = { 0, 1 } }));
        CHECK(srs::overlap(tile::Id { .zoom_level = 1, .coords = { 0, 0 } }, tile::Id { .zoom_level = 3, .coords = { 2, 1 } }));
        CHECK(!srs::overlap(tile::Id { .zoom_level = 1, .coords = { 0, 0 } }, tile::Id { .zoom_level = 3, .coords = { 0, 7 } }));
    }

    SECTION("srs conversion")
    {
        CHECK(srs::lat_long_to_world({ 0, 0 }).x == Approx(0).scale(20037508));
        CHECK(srs::lat_long_to_world({ 0, 0 }).y == Approx(0).scale(20037508));

        constexpr double maxLat = 85.05112878;
        constexpr double maxLong = 180;
        CHECK(srs::lat_long_to_world({ maxLat, maxLong }).x == Approx(20037508.342789244));
        CHECK(srs::lat_long_to_world({ maxLat, maxLong }).y == Approx(20037508.342789244));

        // https://epsg.io/transform#s_srs=4326&t_srs=3857&x=16.3726561&y=48.2086939
        constexpr double westl_hochgrubach_spitze_lat = 48.2086939;
        constexpr double westl_hochgrubach_spitze_long = 16.3726561;
        CHECK(srs::lat_long_to_world({ westl_hochgrubach_spitze_lat, westl_hochgrubach_spitze_long }).x == Approx(1822595.7412222677));
        CHECK(srs::lat_long_to_world({ westl_hochgrubach_spitze_lat, westl_hochgrubach_spitze_long }).y == Approx(6141644.553721141));
    }

    SECTION("srs conversion two way")
    {
        {
            constexpr double lat = 48.2086939;
            constexpr double lon = 16.3726561;
            CHECK(srs::world_to_lat_long(srs::lat_long_to_world({ lat, lon })).x == Approx(lat));
            CHECK(srs::world_to_lat_long(srs::lat_long_to_world({ lat, lon })).y == Approx(lon));
        }
        {
            constexpr double lat = 12.565;
            constexpr double lon = -125.54;
            CHECK(srs::world_to_lat_long(srs::lat_long_to_world({ lat, lon })).x == Approx(lat));
            CHECK(srs::world_to_lat_long(srs::lat_long_to_world({ lat, lon })).y == Approx(lon));
        }
        {
            constexpr double lat = -12.565;
            constexpr double lon = -165.54;
            CHECK(srs::world_to_lat_long(srs::lat_long_to_world({ lat, lon })).x == Approx(lat));
            CHECK(srs::world_to_lat_long(srs::lat_long_to_world({ lat, lon })).y == Approx(lon));
        }
        {
            constexpr double lat = -65.565;
            constexpr double lon = 135.54;
            CHECK(srs::world_to_lat_long(srs::lat_long_to_world({ lat, lon })).x == Approx(lat));
            CHECK(srs::world_to_lat_long(srs::lat_long_to_world({ lat, lon })).y == Approx(lon));
        }
    }

    SECTION("srs conversion with height")
    {
        constexpr double pi = 3.141592653589793238462643383279502884197169399375105820974944;
        CHECK(srs::lat_long_alt_to_world({ 0, 0, 10.0 }).x == Approx(0).scale(20037508));
        CHECK(srs::lat_long_alt_to_world({ 0, 0, 10.0 }).y == Approx(0).scale(20037508));
        CHECK(srs::lat_long_alt_to_world({ 0, 0, 10.0 }).z == Approx(10).scale(20037508));

        {
            constexpr double lat = 48.2086939;
            constexpr double lon = 16.3726561;
            constexpr double alt = 100.0;
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).x == Approx(srs::lat_long_to_world({ lat, lon }).x).scale(1000));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).y == Approx(srs::lat_long_to_world({ lat, lon }).y).scale(1000));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).z >= Approx(alt));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).z == Approx(alt / std::abs(std::cos(lat * pi / 180.0))));
        }

        {
            constexpr double lat = 38.2086939;
            constexpr double lon = -116.3726561;
            constexpr double alt = 200.0;
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).x == Approx(srs::lat_long_to_world({ lat, lon }).x).scale(1000));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).y == Approx(srs::lat_long_to_world({ lat, lon }).y).scale(1000));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).z >= Approx(alt));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).z == Approx(alt / std::abs(std::cos(lat * pi / 180.0))));
        }

        {
            constexpr double lat = -70.2086939;
            constexpr double lon = 26.3726561;
            constexpr double alt = 1000.0;
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).x == Approx(srs::lat_long_to_world({ lat, lon }).x).scale(1000));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).y == Approx(srs::lat_long_to_world({ lat, lon }).y).scale(1000));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).z >= Approx(alt));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).z == Approx(alt / std::abs(std::cos(lat * pi / 180.0))));
        }

        {
            constexpr double lat = -60.2086939;
            constexpr double lon = -96.3726561;
            constexpr double alt = 4000.0;
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).x == Approx(srs::lat_long_to_world({ lat, lon }).x).scale(1000));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).y == Approx(srs::lat_long_to_world({ lat, lon }).y).scale(1000));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).z >= Approx(alt));
            CHECK(srs::lat_long_alt_to_world({ lat, lon, alt }).z == Approx(alt / std::abs(std::cos(lat * pi / 180.0))));
        }

        {
            constexpr double lat = -60.2086939;
            constexpr double lon = -96.3726561;
            constexpr double alt = 4000.0;
            const auto real_coords = glm::dvec3 { lat, lon, alt };
            CHECK(srs::world_to_lat_long_alt(srs::lat_long_alt_to_world(real_coords)).x == Approx(real_coords.x).scale(1000));
            CHECK(srs::world_to_lat_long_alt(srs::lat_long_alt_to_world(real_coords)).y == Approx(real_coords.y).scale(1000));
            CHECK(srs::world_to_lat_long_alt(srs::lat_long_alt_to_world(real_coords)).z == Approx(real_coords.z).scale(1000));
        }
    }
}

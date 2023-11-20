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

#include <QFile>
#include <catch2/catch_test_macros.hpp>

#include "catch2_helpers.h"
#include "nucleus/utils/tile_conversion.h"

namespace {
auto check_alpine_raster_format_for(const glm::u8vec4& v)
{
    const auto float_v = nucleus::utils::tile_conversion::alppineRGBA2float(v);
    const auto ar_v = nucleus::utils::tile_conversion::float2alpineRGBA(float_v);
    CHECK(ar_v == v);
}
auto check_uint16_conversion_for(const glm::u8vec4& v)
{
    const uint16_t short_v = nucleus::utils::tile_conversion::alppineRGBA2uint16(v);
    const auto ar_v = nucleus::utils::tile_conversion::uint162alpineRGBA(short_v);
    CHECK(ar_v == v);
}
}

TEST_CASE("nucleus/utils/tile_conversion")
{

    SECTION("byte array to qimage")
    {
        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "170px-Jeune_bouquetin_de_face.jpg");
        QFile file(filepath);
        file.open(QIODevice::ReadOnly);
        QByteArray ba = file.readAll();
        REQUIRE(ba.size() > 0);
        QImage image = nucleus::utils::tile_conversion::toQImage(ba);
        CHECK(image.size().width() == 170);
        CHECK(image.size().height() == 227);
    }

    SECTION("byte array to raster RGBA")
    {
        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "170px-Jeune_bouquetin_de_face.jpg");
        QFile file(filepath);
        file.open(QIODevice::ReadOnly);
        QByteArray ba = file.readAll();
        REQUIRE(ba.size() > 0);
        const auto raster = nucleus::utils::tile_conversion::toRasterRGBA(ba);
        CHECK(raster.width() == 170);
        CHECK(raster.height() == 227);
        CHECK(raster.buffer().front().x == 80);
        CHECK(raster.buffer().front().y == 92);
        CHECK(raster.buffer().front().z == 92);
        CHECK(raster.buffer().front().w == 255);
    }

    SECTION("float to alpine raster RGBA conversion math")
    {
        const auto one_red = 32.0f;
        const auto one_green = 32.000000001f / 256;
        const auto eps = 0.000000001f;

        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(0) == glm::u8vec4(0, 0, 0, 255));

        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(one_red + eps) == glm::u8vec4(1, 0, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(one_green + eps) == glm::u8vec4(0, 1, 0, 255));

        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(42 * one_red + eps) == glm::u8vec4(42, 0, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(200 * one_green + eps) == glm::u8vec4(0, 200, 0, 255));

        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(255 * one_red + eps) == glm::u8vec4(255, 0, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(253 * one_green + eps) == glm::u8vec4(0, 253, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(254 * one_green + eps) == glm::u8vec4(0, 254, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(255 * one_green + eps) == glm::u8vec4(0, 255, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(256 * one_green + eps) == glm::u8vec4(1, 0, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(257 * one_green + eps) == glm::u8vec4(1, 1, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(258 * one_green + eps) == glm::u8vec4(1, 2, 0, 255));

        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(240 * one_red + 195 * one_green + eps) == glm::u8vec4(240, 195, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(64 * one_red + 255 * one_green + eps) == glm::u8vec4(64, 255, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(255 * one_red + 128 * one_green + eps) == glm::u8vec4(255, 128, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(255 * one_red + 255 * one_green + eps) == glm::u8vec4(255, 255, 0, 255));

        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(123 * one_red + 250 * one_green + eps) == glm::u8vec4(123, 250, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(140 * one_red + 255 * one_green + eps) == glm::u8vec4(140, 255, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(141 * one_red + 0 * one_green + eps) == glm::u8vec4(141, 0, 0, 255));
        CHECK(nucleus::utils::tile_conversion::float2alpineRGBA(141 * one_red + 1 * one_green + eps) == glm::u8vec4(141, 1, 0, 255));
    }
    SECTION("alpine raster RGBA to float conversion math")
    {
        check_alpine_raster_format_for({ 0, 0, 0, 255 });
        check_alpine_raster_format_for({ 0, 1, 0, 255 });
        check_alpine_raster_format_for({ 1, 0, 0, 255 });
        check_alpine_raster_format_for({ 1, 1, 0, 255 });
        check_alpine_raster_format_for({ 1, 100, 0, 255 });
        check_alpine_raster_format_for({ 100, 1, 0, 255 });
        check_alpine_raster_format_for({ 201, 133, 0, 255 });
        check_alpine_raster_format_for({ 0, 255, 0, 255 });
        check_alpine_raster_format_for({ 255, 0, 0, 255 });
        check_alpine_raster_format_for({ 255, 255, 0, 255 });
    }

    SECTION("alpine raster RGBA to unsigned short conversion math")
    {
        check_uint16_conversion_for({ 0, 0, 0, 255 });
        check_uint16_conversion_for({ 0, 1, 0, 255 });
        check_uint16_conversion_for({ 1, 0, 0, 255 });
        check_uint16_conversion_for({ 1, 1, 0, 255 });
        check_uint16_conversion_for({ 1, 100, 0, 255 });
        check_uint16_conversion_for({ 100, 1, 0, 255 });
        check_uint16_conversion_for({ 201, 133, 0, 255 });
        check_uint16_conversion_for({ 0, 255, 0, 255 });
        check_uint16_conversion_for({ 255, 0, 0, 255 });
        check_uint16_conversion_for({ 255, 255, 0, 255 });
    }

    SECTION("byte array to raster unsigned short")
    {
        QString filepath = QString("%1%2").arg(ALP_TEST_DATA_DIR, "test-tile.png");
        QFile file(filepath);
        file.open(QIODevice::ReadOnly);
        QByteArray ba = file.readAll();
        REQUIRE(ba.size() > 0);
        const auto raster = nucleus::utils::tile_conversion::qImage2uint16Raster(nucleus::utils::tile_conversion::toQImage(ba));
        CHECK(raster.width() == 64);
        CHECK(raster.height() == 64);
        CHECK(raster.buffer()[0] == 23 * 256 + 216);
        CHECK(raster.buffer()[1] == 22 * 256 + 33);
    }
}

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

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QThread>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"

using Catch::Approx;
using namespace nucleus::tile_scheduler;

TEST_CASE("nucleus/tile_scheduler/utils/make_bounds altitude correction")
{
    SECTION("root tile") {
        const auto bounds = utils::make_bounds(tile::Id{0, {0, 0}}, 100, 1000);
        CHECK(bounds.min.z == Approx(100).scale(100));
        CHECK(bounds.max.z > 1000);
    }
    SECTION("tile from equator") {
        auto bounds = utils::make_bounds(tile::Id{1, {0, 1}}, 100, 1000); // top left
        CHECK(bounds.min.z == Approx(100).scale(100));
        CHECK(bounds.max.z > 1000);
        bounds = utils::make_bounds(tile::Id{1, {0, 0}}, 100, 1000); // bottom left
        CHECK(bounds.min.z == Approx(100).scale(100));
        CHECK(bounds.max.z > 1000);
    }
}

TEST_CASE("nucleus/tile_scheduler/utils/timestamper")
{
    utils::Timestamper s;
    QThread::msleep(10);
    CHECK(std::abs(int(s.stamp()) - 10) <= 1);

    QThread::msleep(5);
    CHECK(std::abs(int(s.stamp()) - 15) <= 2);
}

TEST_CASE("nucleus/tile_scheduler/utils/TileId2DataMap io")
{
    const auto base_path = std::filesystem::path("./unittests_test_files");
    std::filesystem::remove_all(base_path);
    std::filesystem::create_directories(base_path);
    const auto file_name = base_path / "tile_id_2_data.alp";

    SECTION("interface")
    {
        TileId2DataMap map;
        utils::write_tile_id_2_data_map(map, file_name);
        const auto read_map = utils::read_tile_id_2_data_map(file_name);
        CHECK(read_map.size() == 0);
    }

    SECTION("missing file returns empty map")
    {
        const auto read_map = utils::read_tile_id_2_data_map(base_path / "missing");
        CHECK(read_map.empty());
    }

    SECTION("something written")
    {
        REQUIRE(!std::filesystem::exists(file_name));
        TileId2DataMap map;
        map[tile::Id { 23, { 44, 55 } }] = std::make_shared<QByteArray>("1\02345ABCDabcd");
        utils::write_tile_id_2_data_map(map, file_name);
        CHECK(std::filesystem::exists(file_name));
        CHECK(std::filesystem::file_size(file_name) > std::string("1\02345ABCDabcd").size());
    }

    SECTION("write and read")
    {
        REQUIRE(!std::filesystem::exists(file_name));
        TileId2DataMap map;
        map[tile::Id { 23, { 44, 55 } }] = std::make_shared<QByteArray>("1\02345ABCDabcd");
        map[tile::Id { 1, { 13, 46 } }] = std::make_shared<QByteArray>("1\02345Aabcd");
        map[tile::Id { 2, { 13, 46 } }] = std::make_shared<QByteArray>("");
        REQUIRE(map[tile::Id { 2, { 13, 46 } }]->size() == 0);
        utils::write_tile_id_2_data_map(map, file_name);
        const auto read_map = utils::read_tile_id_2_data_map(file_name);
        REQUIRE(read_map.size() == map.size());
        for (const auto& key_value : map) {
            REQUIRE(read_map.contains(key_value.first));
            CHECK(read_map.at(key_value.first) != key_value.second); // returns a different shared pointer
            CHECK(*read_map.at(key_value.first) == *key_value.second); // bytes are correct
        }
    }

    SECTION("reading a file that is too short returns an empty map")
    {
        std::vector<qint64> bad_sizes = {
            sizeof(size_t) + (sizeof(tile::Id) + sizeof(qsizetype)) * 4 + 4 * 3 + 2, // stop in last read byte array
            68, // stop in read<T> (found by chance)
            sizeof(size_t) + sizeof(tile::Id) + sizeof(qsizetype) + 3, // stop in read byte array (other read byte array)
            sizeof(size_t) + sizeof(tile::Id) + 3
        }; // stop in other read<T>

        REQUIRE(!std::filesystem::exists(file_name));
        TileId2DataMap map;
        map[tile::Id { 23, { 44, 55 } }] = std::make_shared<QByteArray>("1234");
        map[tile::Id { 1, { 13, 46 } }] = std::make_shared<QByteArray>("1234");
        map[tile::Id { 2, { 13, 46 } }] = std::make_shared<QByteArray>("1234");
        map[tile::Id { 3, { 13, 46 } }] = std::make_shared<QByteArray>("1234");
        utils::write_tile_id_2_data_map(map, file_name);
        for (const auto bad_size : bad_sizes) {
            QFile file(file_name);
            auto success = file.open(QIODevice::ReadWrite);
            REQUIRE(success);
            success = file.resize(bad_size);
            REQUIRE(success);
            file.close();

            const auto read_map = utils::read_tile_id_2_data_map(file_name);
            CHECK(read_map.size() == 0);
        }
    }

    SECTION("write and read of a qimage")
    {
        REQUIRE(!std::filesystem::exists(file_name));
        TileId2DataMap map;

        QByteArray jpeg_image_array;
        QByteArray png_image_array;
        {
            QImage image(QSize { int(256), int(256) }, QImage::Format_ARGB32);
            image.fill(Qt::GlobalColor::white);
            QBuffer buffer(&jpeg_image_array);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "JPEG");
        }
        {
            QImage image(QSize { int(64), int(64) }, QImage::Format_ARGB32);
            image.fill(Qt::GlobalColor::white);
            QBuffer buffer(&png_image_array);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");
        }

        const auto jpeg_tile_id = tile::Id { 0, { 0, 0 } };
        const auto png_tile_id = tile::Id { 1, { 0, 0 } };
        map[jpeg_tile_id] = std::make_shared<QByteArray>(jpeg_image_array);
        map[png_tile_id] = std::make_shared<QByteArray>(png_image_array);
        utils::write_tile_id_2_data_map(map, file_name);
        const auto read_map = utils::read_tile_id_2_data_map(file_name);
        REQUIRE(read_map.size() == 2);
        {
            REQUIRE(read_map.contains(jpeg_tile_id));
            REQUIRE(read_map.at(jpeg_tile_id));
            const auto bytes = *read_map.at(jpeg_tile_id);
            const auto read_image = nucleus::utils::tile_conversion::toQImage(*read_map.at(jpeg_tile_id));
            CHECK(read_image.width() == 256);
            CHECK(read_image.height() == 256);
            CHECK(!read_image.isNull());
            CHECK(read_image.constBits());
            CHECK(read_image.pixel({ 55, 33 }) == qRgb(255, 255, 255));
            CHECK(read_image.format() == QImage::Format_RGB32);
        }
        {
            REQUIRE(read_map.contains(png_tile_id));
            REQUIRE(read_map.at(png_tile_id));
            const auto read_png = nucleus::utils::tile_conversion::qImage2uint16Raster(nucleus::utils::tile_conversion::toQImage(*read_map.at(png_tile_id)));
            CHECK(read_png.width() == 64);
            CHECK(read_png.height() == 64);
            CHECK(*(read_png.begin() + 20 * 64 + 30) == 256 * 256 - 1);
        }
    }
}

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

#include "nucleus/tile_scheduler/utils.h"

#include <catch2/catch.hpp>

TEST_CASE("nucleus/tile_scheduler/utils: TileId2DataMap io")
{
    const auto base_path = std::filesystem::path("./unittests_test_files");
    std::filesystem::remove_all(base_path);
    std::filesystem::create_directories(base_path);
    const auto file_name = base_path / "tile_id_2_data.alp";

    SECTION("interface")
    {
        nucleus::tile_scheduler::TileId2DataMap map;
        nucleus::tile_scheduler::utils::write_tile_id_2_data_map(map, file_name);
        const auto read_map = nucleus::tile_scheduler::utils::read_tile_id_2_data_map(file_name);
        CHECK(read_map.size() == 0);
    }

    SECTION("missing file returns empty map")
    {
        const auto read_map = nucleus::tile_scheduler::utils::read_tile_id_2_data_map(base_path / "missing");
        CHECK(read_map.empty());
    }

    SECTION("something written")
    {
        REQUIRE(!std::filesystem::exists(file_name));
        nucleus::tile_scheduler::TileId2DataMap map;
        map[tile::Id { 23, { 44, 55 } }] = std::make_shared<QByteArray>("1\02345ABCDabcd");
        nucleus::tile_scheduler::utils::write_tile_id_2_data_map(map, file_name);
        CHECK(std::filesystem::exists(file_name));
        CHECK(std::filesystem::file_size(file_name) > std::string("1\02345ABCDabcd").size());
    }

    SECTION("write and read")
    {
        REQUIRE(!std::filesystem::exists(file_name));
        nucleus::tile_scheduler::TileId2DataMap map;
        map[tile::Id { 23, { 44, 55 } }] = std::make_shared<QByteArray>("1\02345ABCDabcd");
        map[tile::Id { 1, { 13, 46 } }] = std::make_shared<QByteArray>("1\02345Aabcd");
        nucleus::tile_scheduler::utils::write_tile_id_2_data_map(map, file_name);
        const auto read_map = nucleus::tile_scheduler::utils::read_tile_id_2_data_map(file_name);
        REQUIRE(read_map.size() == map.size());
        for (const auto& key_value : map) {
            REQUIRE(read_map.contains(key_value.first));
            CHECK(read_map.at(key_value.first) != key_value.second); // returns a different shared pointer
            CHECK(*read_map.at(key_value.first) == *key_value.second); // bytes are correct
        }
    }
}

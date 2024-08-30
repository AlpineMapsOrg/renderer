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

#include <memory>

#include <QByteArray>
#include <zpp_bits.h>
#include <catch2/catch_test_macros.hpp>

#include "radix/tile.h"

namespace {

struct TestStruct
{
    tile::Id tile_id;
    std::shared_ptr<QByteArray> name;
};
}
namespace glm {

template<typename T>
constexpr auto serialize(auto & archive, const glm::vec<2, T> & vec)
{
    return archive(vec.x, vec.y);
}

template<typename T>
constexpr auto serialize(auto & archive, glm::vec<2, T> & vec)
{
    return archive(vec.x, vec.y);
}

}

TEST_CASE("zppbits") {
    QByteArray data;
    {
        TestStruct t;
        t.tile_id = {1, {2, 3}};
        t.name.reset(new QByteArray("test"));
        zpp::bits::out out(data);
        out(t).or_throw();
    }
    {
        TestStruct t;
        zpp::bits::in in(data);
        in(t).or_throw();
        CHECK(t.tile_id == tile::Id{1, {2, 3}});
        CHECK(*t.name == "test");
    }
}

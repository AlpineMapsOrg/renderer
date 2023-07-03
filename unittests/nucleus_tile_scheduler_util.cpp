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
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "nucleus/camera/stored_positions.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"
#include "sherpa/quad_tree.h"

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

TEST_CASE("tile_scheduler/utils/refine_functor")
{
    // todo: optimise / benchmark refine functor
    // todo: optimise / benchmark draw list generator
    auto camera = nucleus::camera::stored_positions::stephansdom_closeup();

    QFile file(":/map/height_data.atb");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    const QByteArray data = file.readAll();
    const auto decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(
        TileHeights::deserialise(data));

    const auto refine_functor = utils::refineFunctor(camera, decorator, 1.0);
    const auto all_leaves = quad_tree::onTheFlyTraverse(
        tile::Id{0, {0, 0}},
        [](const tile::Id &v) { return v.zoom_level < 6; },
        [](const tile::Id &v) { return v.children(); });

    std::cout << "num leaves: " << all_leaves.size() << std::endl;
    BENCHMARK("refine functor double")
    {
        auto retval = true;
        for (const auto &id : all_leaves) {
            retval = retval && !refine_functor(id);
        }
        return retval;
    };

    const auto refine_functor_float = utils::refine_functor_float(camera, decorator, 1.0);
    BENCHMARK("refine functor float")
    {
        auto retval = true;
        for (const auto &id : all_leaves) {
            retval = retval && !refine_functor_float(id);
        }
        return retval;
    };
}

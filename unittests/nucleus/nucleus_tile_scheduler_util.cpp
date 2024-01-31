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
#include <nucleus/camera/Definition.h>

#include "nucleus/camera/PositionStorage.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"
#include "radix/quad_tree.h"

using Catch::Approx;
using namespace nucleus::tile_scheduler;

TEST_CASE("nucleus/tile_scheduler/utils/make_bounds altitude correction")
{
    SECTION("root tile") {
        const auto bounds = utils::make_bounds(tile::Id{0, {0, 0}}, 100, 1000);
        CHECK(bounds.min.z == Approx(100 - 0.5).scale(100));
        CHECK(bounds.max.z > 1000);
    }
    SECTION("tile from equator") {
        auto bounds = utils::make_bounds(tile::Id{1, {0, 1}}, 100, 1000); // top left
        CHECK(bounds.min.z == Approx(100 - 0.5).scale(100));
        CHECK(bounds.max.z > 1000);
        bounds = utils::make_bounds(tile::Id{1, {0, 0}}, 100, 1000); // bottom left
        CHECK(bounds.min.z == Approx(100 - 0.5).scale(100));
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
    Q_UNUSED(open);
    const QByteArray data = file.readAll();
    const auto decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(TileHeights::deserialise(data));

    const auto refine_functor = utils::refineFunctor(camera, decorator, 1.0);
    const auto all_leaves = quad_tree::onTheFlyTraverse(
        tile::Id{0, {0, 0}},
        [](const tile::Id &v) { return v.zoom_level < 6; },
        [](const tile::Id &v) { return v.children(); });

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

TEST_CASE("tile_scheduler/utils/camera_frustum_contains_tile")
{
    QFile file(":/map/height_data.atb");
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
    assert(open);
    Q_UNUSED(open);
    const QByteArray data = file.readAll();
    const auto decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(
        TileHeights::deserialise(data));

    SECTION("case 1")
    {
        auto cam = nucleus::camera::Definition(glm::dvec3 { 0, 0, 0 }, glm::dvec3 { 0, 1, 0 });
        cam.set_viewport_size({ 100, 100 });
        cam.set_field_of_view(90);
        cam.set_near_plane(0.01f);

        CHECK(nucleus::tile_scheduler::utils::camera_frustum_contains_tile(cam.frustum(), tile::SrsAndHeightBounds { { -1., 9., -1. }, { 1., 10., 1. } }));
        CHECK(nucleus::tile_scheduler::utils::camera_frustum_contains_tile(cam.frustum(), tile::SrsAndHeightBounds { { 0., 0., 0. }, { 1., 1., 1. } }));
        CHECK(nucleus::tile_scheduler::utils::camera_frustum_contains_tile(cam.frustum(), tile::SrsAndHeightBounds { { -10., -10., -10. }, { 10., 1., 10. } }));
        CHECK(!nucleus::tile_scheduler::utils::camera_frustum_contains_tile(cam.frustum(), tile::SrsAndHeightBounds { { -10., -10., -10. }, { 10., -1., 10. } }));
        CHECK(!nucleus::tile_scheduler::utils::camera_frustum_contains_tile(cam.frustum(), tile::SrsAndHeightBounds { { -10., 0., -10. }, { -9., 1., -9. } }));
    }
    SECTION("case 2")
    {
        auto cam = nucleus::camera::stored_positions::wien();
        cam.set_viewport_size({ 1920, 1080 });
        cam.set_field_of_view(60);

        CHECK(!nucleus::tile_scheduler::utils::camera_frustum_contains_tile_old(cam.frustum(),
            tile::SrsAndHeightBounds { { 1878516.4071364924, 5635549.221409474, 0.0 }, { 2504688.5428486564, 6261721.357121637, 1157.4707507122087 } }));
        CHECK(!nucleus::tile_scheduler::utils::camera_frustum_contains_tile(cam.frustum(),
            tile::SrsAndHeightBounds { { 1878516.4071364924, 5635549.221409474, 0.0 }, { 2504688.5428486564, 6261721.357121637, 1157.4707507122087 } }));

        CHECK(nucleus::tile_scheduler::utils::camera_frustum_contains_tile(cam.frustum(), decorator->aabb({ 3, { 4, 4 } }))
            == nucleus::tile_scheduler::utils::camera_frustum_contains_tile_old(cam.frustum(), decorator->aabb({ 3, { 4, 4 } })));
    }
    SECTION("case 3")
    {
        auto cam = nucleus::camera::Definition({ 1.76106e+06, 6.07163e+06, 2510.08 },
            glm::dvec3 { 1.76106e+06, 6.07163e+06, 2510.08 } - glm::dvec3 { 0.9759, 0.19518, 0.09759 });
        cam.set_viewport_size({ 2561, 1369 });
        cam.set_field_of_view(60);
        CHECK(nucleus::tile_scheduler::utils::camera_frustum_contains_tile(cam.frustum(), decorator->aabb({ 10, { 557, 667 } }))
            == nucleus::tile_scheduler::utils::camera_frustum_contains_tile_old(cam.frustum(), decorator->aabb({ 10, { 557, 667 } })));
    }

    SECTION("many automated test cases")
    {
        std::vector<tile::Id> tile_ids;
        quad_tree::onTheFlyTraverse(
            tile::Id { 0, { 0, 0 } },
            [](const tile::Id& v) { return v.zoom_level < 7; },
            [&tile_ids](const tile::Id& v) {
                tile_ids.push_back(v);
                return v.children();
            });

        QFile file(":/map/height_data.atb");
        const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
        assert(open);
        Q_UNUSED(open);
        const QByteArray data = file.readAll();
        const auto decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(
            TileHeights::deserialise(data));

        const auto camera_positions = std::vector {
            nucleus::camera::stored_positions::karwendel(),
            nucleus::camera::stored_positions::grossglockner(),
            nucleus::camera::stored_positions::oestl_hochgrubach_spitze(),
            nucleus::camera::stored_positions::schneeberg(),
            nucleus::camera::stored_positions::wien(),
            nucleus::camera::stored_positions::stephansdom(),
        };
        for (const auto& camera_position : camera_positions) {
            quad_tree::onTheFlyTraverse(tile::Id { 0, { 0, 0 } },
                nucleus::tile_scheduler::utils::refineFunctor(camera_position,
                    decorator,
                    1),
                [&tile_ids](const tile::Id& v) {
                    tile_ids.push_back(v);
                    return v.children();
                });
        }
        for (const auto& camera : camera_positions) {
            const auto camera_frustum = camera.frustum();
            for (const auto& tile_id : tile_ids) {
                const auto aabb = decorator->aabb(tile_id);
                CHECK(nucleus::tile_scheduler::utils::camera_frustum_contains_tile(camera_frustum, aabb)
                    == nucleus::tile_scheduler::utils::camera_frustum_contains_tile_old(camera_frustum, aabb));
            }
        }

        BENCHMARK("camera_frustum_contains_tile")
        {
            bool retval = false;
            for (const auto& camera : camera_positions) {
                const auto camera_frustum = camera.frustum();
                for (const auto& tile_id : tile_ids) {
                    const auto aabb = decorator->aabb(tile_id);
                    retval = retval != nucleus::tile_scheduler::utils::camera_frustum_contains_tile(camera_frustum, aabb);
                }
            }
            return retval;
        };

        BENCHMARK("camera_frustum_contains_tile_old")
        {
            bool retval = false;
            for (const auto& camera : camera_positions) {
                const auto camera_frustum = camera.frustum();
                for (const auto& tile_id : tile_ids) {
                    const auto aabb = decorator->aabb(tile_id);
                    retval = retval != nucleus::tile_scheduler::utils::camera_frustum_contains_tile_old(camera_frustum, aabb);
                }
            }
            return retval;
        };
    }
}

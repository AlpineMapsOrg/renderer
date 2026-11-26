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


#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "nucleus/camera/Definition.h"
#include "radix/geometry.h"
#include "test_helpers.h"

using test_helpers::equals;
namespace geometry = radix::geometry;

namespace {
glm::vec3 divideByW(const glm::vec4& vec)
{
    return { vec.x / vec.w, vec.y / vec.w, vec.z / vec.w };
}
}

TEST_CASE("nucleus/camera: Definition")
{
    SECTION("constructor")
    {
        {
            const auto c = nucleus::camera::Definition({ 13, 12, 5 }, { 0, 0, 0 });
            {
                const auto t = c.camera_matrix() * glm::vec4 { 0, 0, 0, 1 };
                CHECK(t.x == Approx(0.0f).scale(50));
                CHECK(t.y == Approx(0.0f).scale(50));
                CHECK(t.z < 0); // negative z looks into the scene
            }
            {
                const auto t = c.camera_matrix() * glm::vec4 { 0, 0, 1, 1 };
                CHECK(t.x == Approx(0.0f).scale(50));
                CHECK(t.y > 0); // y (in camera space) points up (in world space)
                CHECK(t.z < 0); // negative z looks into the scene
            }
            {
                const auto t = c.position();
                CHECK(t.x == Approx(13.0f).scale(50));
                CHECK(t.y == Approx(12.0).scale(50));
                CHECK(t.z == Approx(5.0f).scale(50));
            }
        }
        {
            const auto c = nucleus::camera::Definition({ 10, 20, 30 }, { 42, 22, 55 });
            const auto t0 = c.camera_matrix() * glm::vec4 { 42, 22, 55, 1 };
            CHECK(t0.x == Approx(0.0f).scale(50));
            CHECK(t0.y == Approx(0.0f).scale(50));
            CHECK(t0.z < 0); // negative z looks into the scene

            const auto t1 = c.camera_matrix() * glm::vec4 { 42, 22, 100, 1 };
            CHECK(t1.x == Approx(0.0f).scale(50));
            CHECK(t1.y > 0); // y (in camera space) points up (in world space)
            CHECK(t1.z < 0); // negative z looks into the scene
        }
    }
    SECTION("view projection matrix")
    {
        auto c = nucleus::camera::Definition({ 0, 0, 0 }, { 10, 10, 0 });
        c.set_perspective_params(90.0, { 100, 100 }, 1);
        const auto clip_space_v = divideByW(c.local_view_projection_matrix({}) * glm::vec4(10, 10, 0, 1));
        CHECK(clip_space_v.x == Approx(0.0f).scale(50));
        CHECK(clip_space_v.y == Approx(0.0f).scale(50));
        CHECK(clip_space_v.z > 0);
    }
    SECTION("local coordinate system offset")
    {
        // see doc/gl_render_design.svg, this tests that the camera returns the local view projection matrix, i.e., offset such, that the floats are smaller
        auto c = nucleus::camera::Definition({ 1000, 1000, 5 }, { 1000, 1010, 0 });
        c.set_perspective_params(90.0, { 100, 100 }, 1);
        const auto not_transformed = divideByW(c.local_view_projection_matrix({}) * glm::vec4(1000, 1010, 0, 1));
        CHECK(not_transformed.x == Approx(0.0f).scale(50));
        CHECK(not_transformed.y == Approx(0.0f).scale(50));
        CHECK(not_transformed.z > 0);

        const auto transformed = divideByW(c.local_view_projection_matrix(glm::dvec3(1000, 1010, 0)) * glm::vec4(0, 0, 0, 1));
        CHECK(transformed.x == Approx(0.0f).scale(50));
        CHECK(transformed.y == Approx(0.0f).scale(50));
        CHECK(transformed.z > 0);
    }

    SECTION("screen space to glm ndc")
    {
        // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glViewport.xhtml
        auto c = nucleus::camera::Definition({ 1, 2, 3 }, { 10, 2, 3 });
        c.set_viewport_size({ 2, 3 });
        {
            const auto ndc = c.to_ndc({ 1.0, 1.5 });
            CHECK(ndc.x == Approx(0));
            CHECK(ndc.y == Approx(0));
        }
        {
            const auto ndc = c.to_ndc({ 0, 0 });
            CHECK(ndc.x == Approx(-1));
            CHECK(ndc.y == Approx(1));
        }
        {
            const auto ndc = c.to_ndc({ 2, 3 });
            CHECK(ndc.x == Approx(1));
            CHECK(ndc.y == Approx(-1));
        }
        {
            const auto ndc = c.to_ndc({ 1, 1 });
            CHECK(ndc.x == Approx(0.0));
            CHECK(ndc.y == Approx(1.0 / 3.0));
        }
        c.set_viewport_size({ 100, 1000 });
        {
            const auto ndc = c.to_ndc({ 50, 500 });
            CHECK(ndc.x == Approx(0));
            CHECK(ndc.y == Approx(0));
        }
        {
            const auto ndc = c.to_ndc({ 0, 0 });
            CHECK(ndc.x == Approx(-1));
            CHECK(ndc.y == Approx(1));
        }
        {
            const auto ndc = c.to_ndc({ 100, 1000 });
            CHECK(ndc.x == Approx(1));
            CHECK(ndc.y == Approx(-1));
        }
    }

    SECTION("size in screen space")
    {
        const auto to_screen_scale = [](const nucleus::camera::Definition& camera, const glm::dvec4& vec) {
            auto ndc = camera.world_view_projection_matrix() * vec;
            ndc /= ndc.w;
            return glm::dvec2(ndc * 0.5) * glm::dvec2(camera.viewport_size());
        };
        {
            auto c = nucleus::camera::Definition({ 10, 0, 0 }, { 0, 0, 0 });
            c.set_perspective_params(90, { 2, 2 }, 0.1684); // simple test, ndc go from -1 to 1 -> 2 units
            auto p = to_screen_scale(c, glm::dvec4(0, 1, 0, 1));
            CHECK(c.to_screen_space(1, 10) == Approx(p.x)); // camera looks 90deg onto y/z plane, y points right
        }
        {
            auto c = nucleus::camera::Definition({ 10, 0, 0 }, { 0, 0, 0 });
            c.set_perspective_params(30, { 2, 2 }, 0.1684);
            auto p = to_screen_scale(c, glm::dvec4(0, 1, 0, 1));
            CHECK(c.to_screen_space(1, 10) == Approx(p.x));
        }
        {
            auto c = nucleus::camera::Definition({ 10, 0, 0 }, { 0, 0, 0 });
            c.set_perspective_params(30, { 100, 100 }, 0.1684);
            auto p = to_screen_scale(c, glm::dvec4(0, 1, 0, 1));
            CHECK(c.to_screen_space(1, 10) == Approx(p.x));
        }
        {
            auto c = nucleus::camera::Definition({ 10, 0, 0 }, { 0, 0, 0 });
            c.set_perspective_params(30, { 1, 100 }, 0.1684); // camera fov in glm is set for y direction
            auto p = to_screen_scale(c, glm::dvec4(0, 1, 0, 1));
            CHECK(c.to_screen_space(1, 10) == Approx(p.x));
        }
    }

    SECTION("unproject")
    {
        {
            auto c = nucleus::camera::Definition({ 1, 2, 3 }, { 10, 2, 3 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            CHECK(equals(c.ray_direction(glm::vec2(0.0, 0.0)), { 1, 0, 0 }));
            CHECK(equals(glm::dvec2(c.world_view_projection_matrix() * glm::dvec4(19, 2, 3, 1)), glm::dvec2(0.0, 0.0)));

            CHECK(equals(c.ray_direction(glm::vec2(1.0, 0.0)), glm::normalize(glm::dvec3(1, -1, 0))));
            CHECK(equals(c.ray_direction(glm::vec2(-1.0, 0.0)), glm::normalize(glm::dvec3(1, 1, 0))));
            CHECK(equals(c.ray_direction(glm::vec2(1.0, 1.0)), glm::normalize(glm::dvec3(1, -1, 1))));

            CHECK(equals(c.ray_direction(glm::vec2(0.0, 1.0)), glm::normalize(glm::dvec3(1, 0, 1))));
            CHECK(equals(c.ray_direction(glm::vec2(0.0, -1.0)), glm::normalize(glm::dvec3(1, 0, -1))));
        }
        {
            auto c = nucleus::camera::Definition({ 2, 2, 1 }, { 1, 1, 0 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            CHECK(equals(c.ray_direction(glm::vec2(0.0, 0.0)), glm::normalize(glm::dvec3(-1, -1, -1))));
            CHECK(equals(glm::dvec2(c.world_view_projection_matrix() * glm::dvec4(0, 0, -1, 1)), glm::dvec2(0.0, 0.0)));
        }
        {
            auto c = nucleus::camera::Definition({ 1, 1, 1 }, { 0, 0, 0 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            CHECK(equals(c.ray_direction(glm::vec2(0.0, 0.0)), glm::normalize(glm::dvec3(-1, -1, -1))));
        }
        {
            auto c = nucleus::camera::Definition({ 10, 10, 10 }, { 10, 20, 20 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            CHECK(equals(c.ray_direction(glm::vec2(0.0, 0.0)), glm::normalize(glm::dvec3(0, 1, 1))));
        }
        {
            auto c = nucleus::camera::Definition({ 10, 10, 10 }, { 10, 10, 20 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            const auto unprojected = c.ray_direction(glm::vec2(0.0, 0.0));
            CHECK(equals(unprojected, glm::normalize(glm::dvec3(0, 0, 1)), 10));
        }
    }

    SECTION("clipping panes")
    {
        { // camera in origin
            auto c = nucleus::camera::Definition({ 0, 0, 0 }, { 1, 0, 0 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            const auto clipping_panes = c.clipping_planes(); // the order of clipping panes is front, back, top, down, left, and finally right
            REQUIRE(clipping_panes.size() == 6);
            // front and back
            CHECK(equals(clipping_panes[0].normal, glm::normalize(glm::dvec3(1, 0, 0))));
            CHECK(clipping_panes[0].distance == Approx(-0.5));
            CHECK(equals(clipping_panes[1].normal, glm::normalize(glm::dvec3(-1, 0, 0))));
            CHECK(clipping_panes[1].distance == Approx(1'000'000));
            // top and down
            CHECK(equals(clipping_panes[2].normal, glm::normalize(glm::dvec3(1, 0, -1))));
            CHECK(clipping_panes[2].distance == Approx(0).scale(1));
            CHECK(equals(clipping_panes[3].normal, glm::normalize(glm::dvec3(1, 0, 1))));
            CHECK(clipping_panes[3].distance == Approx(0).scale(1));
            // left and right
            CHECK(equals(clipping_panes[4].normal, glm::normalize(glm::dvec3(1, -1, 0))));
            CHECK(clipping_panes[4].distance == Approx(0).scale(1));
            CHECK(equals(clipping_panes[5].normal, glm::normalize(glm::dvec3(1, 1, 0))));
            CHECK(clipping_panes[5].distance == Approx(0).scale(1));
        }
        { // camera somewhere else
            auto c = nucleus::camera::Definition({ 10, 10, 0 }, { 0, 0, 0 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            const auto clipping_panes = c.clipping_planes(); // the order of clipping panes is front, back, top, down, left, and finally right
            REQUIRE(clipping_panes.size() == 6);
            // front and back
            CHECK(equals(clipping_panes[0].normal, glm::normalize(glm::dvec3(-1, -1, 0))));
            CHECK(clipping_panes[0].distance == Approx(std::sqrt(200.0) - 0.5));
            CHECK(equals(clipping_panes[1].normal, glm::normalize(glm::dvec3(1, 1, 0))));
            CHECK(clipping_panes[1].distance == Approx(1'000'000.0 - std::sqrt(200.0)));
            // top and down
            CHECK(equals(clipping_panes[2].normal, glm::normalize(glm::dvec3(-0.5, -0.5, -std::sqrt(0.5)))));
            CHECK(clipping_panes[2].distance == Approx(10).scale(1));
            CHECK(equals(clipping_panes[3].normal, glm::normalize(glm::dvec3(-0.5, -0.5, std::sqrt(0.5)))));
            CHECK(clipping_panes[3].distance == Approx(10).scale(1));
            // left and right
            CHECK(equals(clipping_panes[4].normal, glm::normalize(glm::dvec3(-1, 0, 0))));
            CHECK(clipping_panes[4].distance == Approx(10).scale(1));
            CHECK(equals(clipping_panes[5].normal, glm::normalize(glm::dvec3(0, -1, 0))));
            CHECK(clipping_panes[5].distance == Approx(10).scale(1));
        }
    }

    SECTION("frustum")
    {
        { // camera in origin
            auto c = nucleus::camera::Definition({ 0, 0, 0 }, { 1, 0, 0 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            const auto frustum = c.frustum();

            // planes
            // front and back
            CHECK(equals(frustum.clipping_planes[0].normal, glm::normalize(glm::dvec3(1, 0, 0))));
            CHECK(frustum.clipping_planes[0].distance == Approx(-0.5));
            CHECK(equals(frustum.clipping_planes[1].normal, glm::normalize(glm::dvec3(-1, 0, 0))));
            CHECK(frustum.clipping_planes[1].distance == Approx(1'000'000));
            // top and down
            CHECK(equals(frustum.clipping_planes[2].normal, glm::normalize(glm::dvec3(1, 0, -1))));
            CHECK(frustum.clipping_planes[2].distance == Approx(0).scale(1));
            CHECK(equals(frustum.clipping_planes[3].normal, glm::normalize(glm::dvec3(1, 0, 1))));
            CHECK(frustum.clipping_planes[3].distance == Approx(0).scale(1));
            // left and right
            CHECK(equals(frustum.clipping_planes[4].normal, glm::normalize(glm::dvec3(1, -1, 0))));
            CHECK(frustum.clipping_planes[4].distance == Approx(0).scale(1));
            CHECK(equals(frustum.clipping_planes[5].normal, glm::normalize(glm::dvec3(1, 1, 0))));
            CHECK(frustum.clipping_planes[5].distance == Approx(0).scale(1));

            // corners
            // front plane
            CHECK(equals(frustum.corners[0], glm::dvec3(0.5, 0.5, 0.5)));  // tl
            CHECK(equals(frustum.corners[1], glm::dvec3(0.5, 0.5, -0.5)));  // bl err
            CHECK(equals(frustum.corners[2], glm::dvec3(0.5, -0.5, -0.5)));  // br err
            CHECK(equals(frustum.corners[3], glm::dvec3(0.5, -0.5, 0.5)));  // tr

            // back plane
            CAPTURE(frustum.corners[4].x, frustum.corners[4].y, frustum.corners[4].z);
            CHECK(equals(frustum.corners[4], glm::dvec3(1.0, 1.0, 1.0) * 1'000'000.0)); // tl
            CHECK(equals(frustum.corners[5], glm::dvec3(1.0, 1.0, -1.0) * 1'000'000.0)); // bl
            CHECK(equals(frustum.corners[6], glm::dvec3(1.0, -1.0, -1.0) * 1'000'000.0)); // br
            CHECK(equals(frustum.corners[7], glm::dvec3(1.0, -1.0, 1.0) * 1'000'000.0)); // tr
        }
        { // camera somewhere else
            auto c = nucleus::camera::Definition({ 10, 10, 0 }, { 0, 0, 0 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            const auto frustum = c.frustum();
            // front and back
            CHECK(equals(frustum.clipping_planes[0].normal, glm::normalize(glm::dvec3(-1, -1, 0))));
            CHECK(frustum.clipping_planes[0].distance == Approx(std::sqrt(200.0) - 0.5));
            CHECK(equals(frustum.clipping_planes[1].normal, glm::normalize(glm::dvec3(1, 1, 0))));
            CHECK(frustum.clipping_planes[1].distance == Approx(1'000'000.0 - std::sqrt(200.0)));
            // top and down
            CHECK(equals(frustum.clipping_planes[2].normal, glm::normalize(glm::dvec3(-0.5, -0.5, -std::sqrt(0.5)))));
            CHECK(frustum.clipping_planes[2].distance == Approx(10).scale(1));
            CHECK(equals(frustum.clipping_planes[3].normal, glm::normalize(glm::dvec3(-0.5, -0.5, std::sqrt(0.5)))));
            CHECK(frustum.clipping_planes[3].distance == Approx(10).scale(1));
            // left and right
            CHECK(equals(frustum.clipping_planes[4].normal, glm::normalize(glm::dvec3(-1, 0, 0))));
            CHECK(frustum.clipping_planes[4].distance == Approx(10).scale(1));
            CHECK(equals(frustum.clipping_planes[5].normal, glm::normalize(glm::dvec3(0, -1, 0))));
            CHECK(frustum.clipping_planes[5].distance == Approx(10).scale(1));


            // corners // the order of corners is ccw, starting from top left, front plane -> back plane
            const auto front = frustum.clipping_planes[0];
            const auto back = frustum.clipping_planes[1];
            const auto top = frustum.clipping_planes[2];
            const auto bottom = frustum.clipping_planes[3];
            const auto left = frustum.clipping_planes[4];
            const auto right = frustum.clipping_planes[5];
            // front plane
            CHECK(equals(frustum.corners[0], geometry::intersection(geometry::intersection(left, top).value(), front).value()));  // tl
            CHECK(equals(frustum.corners[1], geometry::intersection(geometry::intersection(left, bottom).value(), front).value()));  // bl
            CHECK(equals(frustum.corners[2], geometry::intersection(geometry::intersection(right, bottom).value(), front).value()));  // br
            CHECK(equals(frustum.corners[3], geometry::intersection(geometry::intersection(right, top).value(), front).value()));  // tr

            // back plane
            CHECK(equals(frustum.corners[4], geometry::intersection(geometry::intersection(left, top).value(), back).value()));  // tl
            CHECK(equals(frustum.corners[5], geometry::intersection(geometry::intersection(left, bottom).value(), back).value()));  // bl
            CHECK(equals(frustum.corners[6], geometry::intersection(geometry::intersection(right, bottom).value(), back).value()));  // br
            CHECK(equals(frustum.corners[7], geometry::intersection(geometry::intersection(right, top).value(), back).value()));  // tr
        }
    }
}

#ifdef QT_GUI_LIB
#include <QBuffer>
#include <QImage>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "nucleus/camera/Controller.h"
#include "nucleus/srs.h"
#include "nucleus/tile/cache_quieries.h"

namespace {

QByteArray png_tile(unsigned size, float altitude)
{
    QImage tile(QSize{int(size), int(size)}, QImage::Format_ARGB32);
    const auto rgb = radix::height_encoding::to_rgb(altitude);
    tile.fill(QColor(rgb.x, rgb.y, rgb.z));
    QByteArray arr;
    QBuffer buffer(&arr);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    tile.save(&buffer, "PNG");
    return arr;
}

// the cache visit descends from the root through cached quads only, so the
// ancestor chain up to the deepest tile must be present
void fill_with_flat_ground(nucleus::tile::MemoryCache& cache, float altitude)
{
    using namespace nucleus::tile;
    const auto quad = [&altitude](const Id& id) {
        const auto children = id.children();
        DataQuad cpu_quad;
        cpu_quad.id = id;
        cpu_quad.n_tiles = 4;
        const auto altitude_tile = png_tile(64, altitude);
        for (unsigned i = 0; i < 4; ++i) {
            cpu_quad.tiles[i].id = children[i];
            cpu_quad.tiles[i].data = std::make_shared<QByteArray>(altitude_tile);
            cpu_quad.tiles[i].network_info.status = NetworkInfo::Status::Good;
            cpu_quad.tiles[i].network_info.timestamp = nucleus::utils::time_since_epoch();
        }
        return cpu_quad;
    };
    cache.insert(quad(Id { 0, { 0, 0 } }));
    cache.insert(quad(Id { 1, { 1, 1 } }));
    cache.insert(quad(Id { 2, { 2, 2 } }));
    cache.insert(quad(Id { 3, { 4, 5 } }));
    cache.insert(quad(Id { 4, { 8, 10 } }));
}

nucleus::camera::Definition camera_at(const glm::dvec2& lat_long, double altitude)
{
    nucleus::camera::Definition camera;
    const glm::dvec3 position = nucleus::srs::lat_long_alt_to_world({ lat_long.x, lat_long.y, altitude });
    camera.look_at(position, position + glm::dvec3(0.0, -1000.0, -500.0));
    return camera;
}

} // namespace

TEST_CASE("Controller terrain floor")
{
    using namespace nucleus::camera;
    // inside the cached tile chain, ground is a flat 1000 m
    const glm::dvec2 here = { 47.5587933, 12.3450985 };
    nucleus::tile::MemoryCache cache;
    fill_with_flat_ground(cache, 1000.f);

    SECTION("a camera below the dem is lifted to 20 m above it")
    {
        nucleus::tile::cache_queries::DemMeshSampler sampler(&cache);
        Controller controller(camera_at(here, 900.0), nullptr, nullptr, &sampler);
        Definition emitted;
        QObject::connect(&controller, &Controller::definition_changed, [&emitted](const Definition& d) { emitted = d; });
        controller.set_pixel_error_threshold(3.0f); // any change triggers update()

        const glm::dvec3 lat_long_alt = nucleus::srs::world_to_lat_long_alt(emitted.position());
        CHECK(lat_long_alt.z == Catch::Approx(1020.0).margin(0.5));
        // vertical lift only: the horizontal position is untouched
        CHECK(lat_long_alt.x == Catch::Approx(here.x).margin(1e-6));
        CHECK(lat_long_alt.y == Catch::Approx(here.y).margin(1e-6));
    }

    SECTION("a camera above the dem is not moved")
    {
        nucleus::tile::cache_queries::DemMeshSampler sampler(&cache);
        Controller controller(camera_at(here, 1500.0), nullptr, nullptr, &sampler);
        Definition emitted;
        QObject::connect(&controller, &Controller::definition_changed, [&emitted](const Definition& d) { emitted = d; });
        controller.set_pixel_error_threshold(3.0f);

        const glm::dvec3 lat_long_alt = nucleus::srs::world_to_lat_long_alt(emitted.position());
        CHECK(lat_long_alt.z == Catch::Approx(1500.0).margin(0.5));
    }

    SECTION("without a dem at the camera position the floor is skipped")
    {
        nucleus::tile::MemoryCache empty_cache;
        nucleus::tile::cache_queries::DemMeshSampler sampler(&empty_cache);
        Controller controller(camera_at(here, 900.0), nullptr, nullptr, &sampler);
        Definition emitted;
        QObject::connect(&controller, &Controller::definition_changed, [&emitted](const Definition& d) { emitted = d; });
        controller.set_pixel_error_threshold(3.0f);

        const glm::dvec3 lat_long_alt = nucleus::srs::world_to_lat_long_alt(emitted.position());
        CHECK(lat_long_alt.z == Catch::Approx(900.0).margin(0.5));
    }

    SECTION("frame_area settles south of and above the area, looking at it")
    {
        nucleus::tile::cache_queries::DemMeshSampler sampler(&cache);
        Controller controller(camera_at(here, 3000.0), nullptr, nullptr, &sampler);
        // a small route-shaped area around `here`, running east-west
        const glm::dvec2 c = nucleus::srs::lat_long_to_world(here);
        controller.frame_area(c - glm::dvec2(500.0, 10.0), c + glm::dvec2(500.0, 10.0), 1000.0, 1200.0);
        // run the animation out (~500 ms)
        glm::dvec3 position(0.0);
        for (int i = 0; i < 30; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            controller.advance_camera();
            position = controller.definition().position();
        }
        for (int i = 0; i < 3; ++i) { // settle
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            controller.advance_camera();
            position = controller.definition().position();
        }

        const glm::dvec3 lat_long_alt = nucleus::srs::world_to_lat_long_alt(position);
        CHECK(lat_long_alt.z >= 1500.0); // 500 m above the flat 1000 m ground
        CHECK(position.y < c.y); // camera sits south of the route
        // and looks at the area centre (the z axis is the camera's BACKWARD
        // axis, so it must point exactly away from the centre)
        const glm::dvec3 centre_world = nucleus::srs::lat_long_alt_to_world({ here.x, here.y, 1100.0 });
        const glm::dvec3 to_centre = glm::normalize(centre_world - position);
        CHECK(glm::dot(controller.definition().z_axis(), to_centre) < -0.99);
    }

    SECTION("without a terrain cache the floor is inert")
    {
        Controller controller(camera_at(here, 900.0), nullptr, nullptr, nullptr);
        Definition emitted;
        QObject::connect(&controller, &Controller::definition_changed, [&emitted](const Definition& d) { emitted = d; });
        controller.set_pixel_error_threshold(3.0f);

        const glm::dvec3 lat_long_alt = nucleus::srs::world_to_lat_long_alt(emitted.position());
        CHECK(lat_long_alt.z == Catch::Approx(900.0).margin(0.5));
    }
}
#endif

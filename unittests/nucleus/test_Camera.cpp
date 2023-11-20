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

#include "nucleus/camera/Definition.h"
#include "radix/geometry.h"
#include "test_helpers.h"

using test_helpers::equals;

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
            CHECK(clipping_panes[1].distance == Approx(500'000));
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
            CHECK(clipping_panes[1].distance == Approx(500'000.0 - std::sqrt(200.0)));
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
            CHECK(frustum.clipping_planes[1].distance == Approx(500'000));
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
            CHECK(equals(frustum.corners[4], glm::dvec3(0.5, 0.5, 0.5) * 1'000'000.0));  // tl
            CHECK(equals(frustum.corners[5], glm::dvec3(0.5, 0.5, -0.5) * 1'000'000.0));  // bl
            CHECK(equals(frustum.corners[6], glm::dvec3(0.5, -0.5, -0.5) * 1'000'000.0));  // br
            CHECK(equals(frustum.corners[7], glm::dvec3(0.5, -0.5, 0.5) * 1'000'000.0));  // tr
        }
        { // camera somewhere else
            auto c = nucleus::camera::Definition({ 10, 10, 0 }, { 0, 0, 0 });
            c.set_perspective_params(90, { 100, 100 }, 0.5);
            const auto frustum = c.frustum();
            // front and back
            CHECK(equals(frustum.clipping_planes[0].normal, glm::normalize(glm::dvec3(-1, -1, 0))));
            CHECK(frustum.clipping_planes[0].distance == Approx(std::sqrt(200.0) - 0.5));
            CHECK(equals(frustum.clipping_planes[1].normal, glm::normalize(glm::dvec3(1, 1, 0))));
            CHECK(frustum.clipping_planes[1].distance == Approx(500'000.0 - std::sqrt(200.0)));
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

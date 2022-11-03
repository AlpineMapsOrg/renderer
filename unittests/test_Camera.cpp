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

#include "alpine_renderer/Camera.h"

#include <catch2/catch.hpp>

#include "unittests/test_helpers.h"
#include "alpine_renderer/utils/geometry.h"

using test_helpers::equals;

namespace {
glm::vec3 divideByW(const glm::vec4& vec) {
  return {vec.x / vec.w, vec.y / vec.w, vec.z / vec.w};
}
}

TEST_CASE("Camera") {
  SECTION("constructor") {
    {
      const auto c = Camera({13, 12, 5}, {0, 0, 0});
      {
        const auto t = c.cameraMatrix() * glm::vec4{0, 0, 0, 1};
        CHECK(t.x == Approx(0.0f).scale(50));
        CHECK(t.y == Approx(0.0f).scale(50));
        CHECK(t.z < 0); // negative z looks into the scene
      }
      {
        const auto t = c.cameraMatrix() * glm::vec4{0, 0, 1, 1};
        CHECK(t.x == Approx(0.0f).scale(50));
        CHECK(t.y > 0);  // y (in camera space) points up (in world space)
        CHECK(t.z < 0);  // negative z looks into the scene
      }
      {
        const auto t = c.position();
        CHECK(t.x == Approx(13.0f).scale(50));
        CHECK(t.y == Approx(12.0).scale(50));
        CHECK(t.z == Approx(5.0f).scale(50));
      }
    }
    {
      const auto c = Camera({10, 20, 30}, {42, 22, 55});
      const auto t0 = c.cameraMatrix() * glm::vec4{42, 22, 55, 1};
      CHECK(t0.x == Approx(0.0f).scale(50));
      CHECK(t0.y == Approx(0.0f).scale(50));
      CHECK(t0.z < 0); // negative z looks into the scene

      const auto t1 = c.cameraMatrix() * glm::vec4{42, 22, 100, 1};
      CHECK(t1.x == Approx(0.0f).scale(50));
      CHECK(t1.y > 0);  // y (in camera space) points up (in world space)
      CHECK(t1.z < 0);  // negative z looks into the scene
    }
  }
  SECTION("view projection matrix") {
    auto c = Camera({0, 0, 0}, {10, 10, 0});
    c.setPerspectiveParams(90.0, {100, 100}, 1);
    const auto clip_space_v = divideByW(c.localViewProjectionMatrix({}) * glm::vec4(10, 10, 0, 1));
    CHECK(clip_space_v.x == Approx(0.0f).scale(50));
    CHECK(clip_space_v.y == Approx(0.0f).scale(50));
    CHECK(clip_space_v.z > 0);
  }
  SECTION("local coordinate system offset") {
    // see doc/gl_render_design.svg, this tests that the camera returns the local view projection matrix, i.e., offset such, that the floats are smaller
    auto c = Camera({1000, 1000, 5}, {1000, 1010, 0});
    c.setPerspectiveParams(90.0, {100, 100}, 1);
    const auto not_transformed = divideByW(c.localViewProjectionMatrix({}) * glm::vec4(1000, 1010, 0, 1));
    CHECK(not_transformed.x == Approx(0.0f).scale(50));
    CHECK(not_transformed.y == Approx(0.0f).scale(50));
    CHECK(not_transformed.z > 0);

    const auto transformed = divideByW(c.localViewProjectionMatrix(glm::dvec3(1000, 1010, 0)) * glm::vec4(0, 0, 0, 1));
    CHECK(transformed.x == Approx(0.0f).scale(50));
    CHECK(transformed.y == Approx(0.0f).scale(50));
    CHECK(transformed.z > 0);
  }
  SECTION("unproject") {
    {
      auto c = Camera({1, 2, 3}, {10, 2, 3});
      c.setPerspectiveParams(90, {100, 100}, 0.5);
      CHECK(equals(c.ray_direction(glm::vec2(0.0, 0.0)), {1, 0, 0}));
      CHECK(equals(glm::dvec2(c.worldViewProjectionMatrix() * glm::dvec4(19, 2, 3, 1)), glm::dvec2(0.0, 0.0)));

      CHECK(equals(c.ray_direction(glm::vec2(1.0, 0.0)), glm::normalize(glm::dvec3(1, -1, 0))));
      CHECK(equals(c.ray_direction(glm::vec2(-1.0, 0.0)), glm::normalize(glm::dvec3(1, 1, 0))));
      CHECK(equals(c.ray_direction(glm::vec2(1.0, 1.0)), glm::normalize(glm::dvec3(1, -1, 1))));

      CHECK(equals(c.ray_direction(glm::vec2(0.0, 1.0)), glm::normalize(glm::dvec3(1, 0, 1))));
      CHECK(equals(c.ray_direction(glm::vec2(0.0, -1.0)), glm::normalize(glm::dvec3(1, 0, -1))));
    }
    {
      auto c = Camera({2, 2, 1}, {1, 1, 0});
      c.setPerspectiveParams(90, {100, 100}, 0.5);
      CHECK(equals(c.ray_direction(glm::vec2(0.0, 0.0)), glm::normalize(glm::dvec3(-1, -1, -1))));
      CHECK(equals(glm::dvec2(c.worldViewProjectionMatrix() * glm::dvec4(0, 0, -1, 1)), glm::dvec2(0.0, 0.0)));
    }
    {
      auto c = Camera({1, 1, 1}, {0, 0, 0});
      c.setPerspectiveParams(90, {100, 100}, 0.5);
      CHECK(equals(c.ray_direction(glm::vec2(0.0, 0.0)), glm::normalize(glm::dvec3(-1, -1, -1))));
    }
    {
      auto c = Camera({10, 10, 10}, {10, 20, 20});
      c.setPerspectiveParams(90, {100, 100}, 0.5);
      CHECK(equals(c.ray_direction(glm::vec2(0.0, 0.0)), glm::normalize(glm::dvec3(0, 1, 1))));
    }
    {
      auto c = Camera({10, 10, 10}, {10, 10, 20});
      c.setPerspectiveParams(90, {100, 100}, 0.5);
      const auto unprojected = c.ray_direction(glm::vec2(0.0, 0.0));
      CHECK(equals(unprojected, glm::normalize(glm::dvec3(0, 0, 1)), 10));
    }

  }

  SECTION("clipping panes") {
    { // camera in origin
      auto c = Camera({0, 0, 0}, {1, 0, 0});
      c.setPerspectiveParams(90, {100, 100}, 0.5);
      const auto clipping_panes = c.clippingPlanes();  // the order of clipping panes is front, back, top, down, left, and finally right
      REQUIRE(clipping_panes.size() == 6);
      // front and back
      CHECK(equals(clipping_panes[0].normal, glm::normalize(glm::dvec3(1, 0, 0))));
      CHECK(clipping_panes[0].distance == Approx(-0.5));
      CHECK(equals(clipping_panes[1].normal, glm::normalize(glm::dvec3(-1, 0, 0))));
      CHECK(clipping_panes[1].distance == Approx(500));
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
      auto c = Camera({10, 10, 0}, {0, 0, 0});
      c.setPerspectiveParams(90, {100, 100}, 0.5);
      const auto clipping_panes = c.clippingPlanes();  // the order of clipping panes is front, back, top, down, left, and finally right
      REQUIRE(clipping_panes.size() == 6);
      // front and back
      CHECK(equals(clipping_panes[0].normal, glm::normalize(glm::dvec3(-1, -1, 0))));
      CHECK(clipping_panes[0].distance == Approx(std::sqrt(200.0) - 0.5));
      CHECK(equals(clipping_panes[1].normal, glm::normalize(glm::dvec3(1, 1, 0))));
      CHECK(clipping_panes[1].distance == Approx(500.0 - std::sqrt(200.0)));
      // top and down
      CHECK(equals(clipping_panes[2].normal, glm::normalize(glm::dvec3(-0.5, -0.5, - std::sqrt(0.5)))));
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
}

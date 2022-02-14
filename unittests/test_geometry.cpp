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

#include "render_backend/utils/geometry.h"

#include <catch2/catch.hpp>

TEST_CASE("geometry") {
  SECTION("aabb") {
    AABB<3, double> box = {.min = {0.0, 1.0, 2.0}, .max={10.0, 11.0, 12.0}};
    CHECK(geometry::inside(glm::dvec3(5, 5, 5), box));
    CHECK(!geometry::inside(glm::dvec3(5, 15, 5), box));

    CHECK(geometry::centroid(box).x == Approx(5.0));
    CHECK(geometry::centroid(box).y == Approx(6.0));
    CHECK(geometry::centroid(box).z == Approx(7.0));
  }

  SECTION("aabb to triangles") {
    AABB<3, double> box = {.min = {0.0, 1.0, 2.0}, .max={10.0, 11.0, 12.0}};
    const auto centroid = geometry::centroid(box);
    const auto triangles = geometry::triangulise(box);
    unsigned cnt_minx = 0;
    unsigned cnt_miny = 0;
    unsigned cnt_minz = 0;
    unsigned cnt_maxx = 0;
    unsigned cnt_maxy = 0;
    unsigned cnt_maxz = 0;
    REQUIRE(triangles.size() == 6u * 2u);
    for (const auto& tri : triangles) {
      for (unsigned i = 0; i < 3; ++i) {
        CHECK(((tri[i].x == 0.0 || tri[i].x == 10.0) && (tri[i].y == 1.0 || tri[i].y == 11.0) && (tri[i].z == 2.0 || tri[i].z == 12.0)));
        if (tri[i].x == 0.0) cnt_minx++;
        if (tri[i].x == 10.0) cnt_maxx++;
        if (tri[i].y == 1.0) cnt_miny++;
        if (tri[i].y == 11.0) cnt_maxy++;
        if (tri[i].z == 2.0) cnt_minz++;
        if (tri[i].z == 12.0) cnt_maxz++;
      }
      const auto ab = tri[1] - tri[0];
      const auto ac = tri[2] - tri[0];
      const auto normal = glm::cross(glm::normalize(ab), glm::normalize(ac));
      const auto centroid_a = glm::normalize(tri[0] - centroid);
      const auto centroid_b = glm::normalize(tri[1] - centroid);
      const auto centroid_c = glm::normalize(tri[2] - centroid);

      CHECK(glm::dot(normal, centroid_a) > 0);
      CHECK(glm::dot(normal, centroid_b) > 0);
      CHECK(glm::dot(normal, centroid_c) > 0);
    }
    CHECK(cnt_minx == 3 + 3 + 3 * 4);
    CHECK(cnt_miny == 3 + 3 + 3 * 4);
    CHECK(cnt_minz == 3 + 3 + 3 * 4);

    CHECK(cnt_maxx == 3 + 3 + 3 * 4);
    CHECK(cnt_maxy == 3 + 3 + 3 * 4);
    CHECK(cnt_maxz == 3 + 3 + 3 * 4);
  }

  // turn aabb into list of triangles
  // transform triangles with camera
  // clip
  // from clipped triangles, take the closest point
  // compute point 1px away parallel to screen
  // transform back, subtract, compute length
  // compare with box side length -> more than 1/256 -> we need to subdivide
}

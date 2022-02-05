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

#include "render_backend/Camera.h"

#include <catch2/catch.hpp>


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
}

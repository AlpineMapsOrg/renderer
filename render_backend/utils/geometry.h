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

#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>

template <glm::length_t n_dimensions, typename T>
struct AABB {
  glm::vec<n_dimensions, T> min;
  glm::vec<n_dimensions, T> max;
};

template <glm::length_t n_dimensions, typename T>
using Triangle = std::array<glm::vec<n_dimensions, T>, 3>;


namespace geometry {
template <glm::length_t n_dimensions, typename T>
bool inside(const glm::vec<n_dimensions, T>& point, const AABB<n_dimensions, T>& box) {
  return glm::all(glm::lessThan(box.min, point) && glm::lessThan(point, box.max));
}

template <glm::length_t n_dimensions, typename T>
glm::vec<n_dimensions, T> centroid(const AABB<n_dimensions, T>& box) {
  return (box.max + box.min) * T(0.5);
}


template <typename T>
std::vector<Triangle<3, T>> triangulise(const AABB<3, T>& box) {
  using Tri = Triangle<3, T>;
  using Vert = glm::vec<3, T>;

  // counter clockwise top face
  const auto a = Vert{box.min.x, box.min.y, box.max.z};
  const auto b = Vert{box.max.x, box.min.y, box.max.z};
  const auto c = Vert{box.max.x, box.max.y, box.max.z};
  const auto d = Vert{box.min.x, box.max.y, box.max.z};
  // ccw bottom face, normal points up
  const auto e = Vert{box.min.x, box.min.y, box.min.z};
  const auto f = Vert{box.max.x, box.min.y, box.min.z};
  const auto g = Vert{box.max.x, box.max.y, box.min.z};
  const auto h = Vert{box.min.x, box.max.y, box.min.z};

  return std::vector<Tri>({
    {a, b, c}, {a, c, d},
    {a, e, b}, {b, e, f},
    {b, f, c}, {c, f, g},
    {c, g, d}, {d, g, h},
    {d, h, a}, {a, h, e},
    {e, g, f}, {e, h, g}
  });
}

}

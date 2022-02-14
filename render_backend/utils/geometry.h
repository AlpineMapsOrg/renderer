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

namespace geometry {
// types
template <glm::length_t n_dimensions, typename T>
struct AABB {
  glm::vec<n_dimensions, T> min;
  glm::vec<n_dimensions, T> max;
};

template <glm::length_t n_dimensions, typename T>
using Triangle = std::array<glm::vec<n_dimensions, T>, 3>;

template <glm::length_t n_dimensions, typename T>
using Edge = std::array<glm::vec<n_dimensions, T>, 2>;

template <typename T>
struct Plane {
  glm::vec<3, T> normal;
  T distance;
};

// functions
template <glm::length_t n_dimensions, typename T>
bool inside(const glm::vec<n_dimensions, T>& point, const AABB<n_dimensions, T>& box) {
  return glm::all(glm::lessThan(box.min, point) && glm::lessThan(point, box.max));
}

template<typename T>
glm::tvec3<T> normal(const Triangle<3, T>& triangle) {
  const auto ab = triangle[1] - triangle[0];
  const auto ac = triangle[2] - triangle[0];
  return glm::normalize(glm::cross(glm::normalize(ab), glm::normalize(ac)));
}

template <glm::length_t n_dimensions, typename T>
glm::vec<n_dimensions, T> centroid(const AABB<n_dimensions, T>& box) {
  return (box.max + box.min) * T(0.5);
}

template <typename T>
T distance(const Plane<T>& plane, const glm::tvec3<T>& point) {
  // from https://gabrielgambetta.com/computer-graphics-from-scratch/11-clipping.html
  return glm::dot(plane.normal, point) + plane.distance;
}

template <typename T>
glm::tvec3<T> intersect(const Edge<3, T>& line, const Plane<T>& plane) {
  // from https://gabrielgambetta.com/computer-graphics-from-scratch/11-clipping.html
  const auto b_minus_a = line[1] - line[0];
  T t = (-plane.distance - glm::dot(plane.normal, line[0])) / glm::dot(plane.normal, b_minus_a);
  return line[0] + t * b_minus_a;
}

template <typename T>
std::vector<Triangle<3, T>> clip(const Triangle<3, T>& triangle, const Plane<T>& plane) {
  using Tri = Triangle<3, T>;
  const auto triangles_2in = [&plane](const glm::tvec3<T>& inside_a, const glm::tvec3<T>& inside_b, const glm::tvec3<T>& outside_c) {
    const auto a_prime = intersect({inside_a, outside_c}, plane);
    const auto b_prime = intersect({inside_b, outside_c}, plane);
    return std::vector<Tri>({Tri{inside_a, inside_b, b_prime}, Tri{inside_a,  b_prime, a_prime}});
  };

  const auto b0 = distance(plane, triangle[0]) > 0;
  const auto b1 = distance(plane, triangle[1]) > 0;
  const auto b2 = distance(plane, triangle[2]) > 0;
  const auto s = b0 + b1 + b2;
  if (s == 3)
    return {triangle};
  if (s == 2) {
    if (!b0) return triangles_2in(triangle[1], triangle[2], triangle[0]);
    if (!b1) return triangles_2in(triangle[2], triangle[0], triangle[1]);
    if (!b2) return triangles_2in(triangle[0], triangle[1], triangle[2]);
    assert(false);
  }
  if (s == 1) {
    if (b0) return {{triangle[0], intersect({triangle[0], triangle[1]}, plane), intersect({triangle[0], triangle[2]}, plane)}};
    if (b1) return {{intersect({triangle[0], triangle[1]}, plane), triangle[1], intersect({triangle[1], triangle[2]}, plane)}};
    if (b2) return {{intersect({triangle[0], triangle[2]}, plane), intersect({triangle[1], triangle[2]}, plane), triangle[2]}};
    assert(false);
  }
  return {};
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

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

#include "TileScheduler.h"

#include "render_backend/srs.h"
#include "render_backend/utils/geometry.h"
#include "render_backend/utils/QuadTree.h"

namespace {
geometry::AABB<3, double> aabb(const srs::TileId& tile_id)
{
  const auto bounds = srs::tile_bounds(tile_id);
  return {.min = {bounds.min, 0}, .max = {bounds.max, 4000}};
}

glm::dvec3 nearestPoint(const std::vector<geometry::Triangle<3, double>>& triangles, const Camera& camera) {
  const auto camera_distance = [&camera](const auto& point) {
    const auto delta = point - camera.position();
    return glm::dot(delta, delta);
  };
  glm::dvec3 nearest_point = triangles.front().front();
  auto nearest_distance = camera_distance(nearest_point);
  for (const auto& triangle : triangles) {
    for (const auto& point : triangle) {
      const auto current_distance = camera_distance(point);
      if (current_distance < nearest_distance) {
        nearest_point = point;
        nearest_distance = current_distance;
      }
    }
  }
  return nearest_point;
}

}

TileScheduler::TileScheduler()
{

}

std::vector<srs::TileId> TileScheduler::loadCandidates(const Camera& camera) const
{
  const auto refine = [this, camera](const srs::TileId& tile) {
    if (tile.zoom_level >= 16)
      return false;

    const auto tile_aabb = aabb(tile);
    const auto triangles = geometry::clip(geometry::triangulise(tile_aabb), camera.clippingPlanes());
    if (triangles.empty())
      return false;
    const auto nearest_point = glm::dvec4(nearestPoint(triangles, camera), 1);
    const auto aabb_width = tile_aabb.max.x - tile_aabb.min.x;
    const auto other_point = nearest_point + glm::dvec4(camera.xAxis() * aabb_width / 256.0, 0);
    const auto vp_mat = camera.worldViewProjectionMatrix();

    const auto screen_space_difference = length((vp_mat * nearest_point - vp_mat * other_point).xy());
    if (screen_space_difference < 2.0 / camera.viewportSize().x)
      return false;
    return true;
  };
  return quad_tree::onTheFlyTraverse(srs::TileId{0, {0, 0}}, refine, srs::subtiles);
}

void TileScheduler::updateCamera(const Camera& camera)
{
  const auto tiles = loadCandidates(camera);
  for (const auto& t : tiles)
    emit tileRequested(t);
}

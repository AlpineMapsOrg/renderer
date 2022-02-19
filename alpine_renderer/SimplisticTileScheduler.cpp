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

#include "SimplisticTileScheduler.h"

#include "alpine_renderer/Tile.h"
#include "alpine_renderer/srs.h"
#include "alpine_renderer/utils/geometry.h"
#include "alpine_renderer/utils/QuadTree.h"
#include "alpine_renderer/utils/tile_conversion.h"

namespace {
geometry::AABB<3, double> aabb(const srs::TileId& tile_id)
{
  const auto bounds = srs::tile_bounds(tile_id);
  return {.min = {bounds.min, 100.0}, .max = {bounds.max, 4000}};
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

SimplisticTileScheduler::SimplisticTileScheduler()
{

}

std::vector<srs::TileId> SimplisticTileScheduler::loadCandidates(const Camera& camera)
{
  const auto refine = [camera](const srs::TileId& tile) {
    if (tile.zoom_level >= 15)
      return false;

    const auto tile_aabb = aabb(tile);
    const auto triangles = geometry::clip(geometry::triangulise(tile_aabb), camera.clippingPlanes());
    if (triangles.empty())
      return false;
    const auto nearest_point = glm::dvec4(nearestPoint(triangles, camera), 1);
    const auto aabb_width = tile_aabb.max.x - tile_aabb.min.x;
    const auto other_point = nearest_point + glm::dvec4(camera.xAxis() * aabb_width / 256.0, 0);
    const auto vp_mat = camera.worldViewProjectionMatrix();

    auto nearest_screenspace = vp_mat * nearest_point;
    nearest_screenspace /= nearest_screenspace.w;
    auto other_screenspace = vp_mat * other_point;
    other_screenspace /= other_screenspace.w;
    const auto clip_space_difference = length((nearest_screenspace - other_screenspace).xy());

    return clip_space_difference * 0.5 * camera.viewportSize().x >= 4.0;
  };
  return quad_tree::onTheFlyTraverse(srs::TileId{0, {0, 0}}, refine, [](const auto& v) { return srs::subtiles(v); });
}

size_t SimplisticTileScheduler::numberOfTilesInTransit() const
{
  return m_pending_tile_requests.size();
}

size_t SimplisticTileScheduler::numberOfWaitingHeightTiles() const
{
  return m_loaded_height_tiles.size();
}

size_t SimplisticTileScheduler::numberOfWaitingOrthoTiles() const
{
  return m_loaded_ortho_tiles.size();
}

SimplisticTileScheduler::TileSet SimplisticTileScheduler::gpuTiles() const
{
  return m_gpu_tiles;
}

void SimplisticTileScheduler::updateCamera(const Camera& camera)
{
  if (!enabled())
    return;
  const auto tiles = loadCandidates(camera);
  auto expired_gpu_tiles = m_gpu_tiles;
  for (const auto& t : tiles) {
    if (m_pending_tile_requests.contains(t))
      continue;
    if (m_gpu_tiles.contains(t)) {
      expired_gpu_tiles.erase(t);
      continue;
    }
    m_pending_tile_requests.insert(t);
    emit tileRequested(t);
  }
  for (const auto& t : expired_gpu_tiles) {
    m_gpu_tiles.erase(t);
    emit tileExpired(t);
  }
}

void SimplisticTileScheduler::receiveOrthoTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
  m_loaded_ortho_tiles[tile_id] = data;
  checkLoadedTile(tile_id);
}

void SimplisticTileScheduler::receiveHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
  m_loaded_height_tiles[tile_id] = data;
  checkLoadedTile(tile_id);
}

void SimplisticTileScheduler::checkLoadedTile(const srs::TileId& tile_id)
{
  if (m_loaded_height_tiles.contains(tile_id) && m_loaded_ortho_tiles.contains(tile_id)) {
    m_pending_tile_requests.erase(tile_id);
    auto heightraster = tile_conversion::qImage2uint16Raster(tile_conversion::toQImage(*m_loaded_height_tiles[tile_id]));
    auto ortho = tile_conversion::toQImage(*m_loaded_ortho_tiles[tile_id]);
    const auto tile = std::make_shared<Tile>(tile_id, srs::tile_bounds(tile_id), std::move(heightraster), std::move(ortho));
    m_loaded_ortho_tiles.erase(tile_id);
    m_loaded_height_tiles.erase(tile_id);
    m_gpu_tiles.insert(tile_id);
    emit tileReady(tile);
  }
}

bool SimplisticTileScheduler::enabled() const
{
  return m_enabled;
}

void SimplisticTileScheduler::setEnabled(bool newEnabled)
{
  m_enabled = newEnabled;
}

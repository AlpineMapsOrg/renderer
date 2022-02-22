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

#include "alpine_renderer/tile_scheduler/SimplisticTileScheduler.h"
#include "alpine_renderer/tile_scheduler/utils.h"

#include "alpine_renderer/Tile.h"
#include "alpine_renderer/srs.h"
#include "alpine_renderer/utils/geometry.h"
#include "alpine_renderer/utils/QuadTree.h"
#include "alpine_renderer/utils/tile_conversion.h"


SimplisticTileScheduler::SimplisticTileScheduler() = default;

std::vector<srs::TileId> SimplisticTileScheduler::loadCandidates(const Camera& camera)
{
//  return quad_tree::onTheFlyTraverse(srs::TileId{0, {0, 0}}, tile_scheduler::refineFunctor(camera, 1.0), [](const auto& v) { return srs::subtiles(v); });
  const auto all_leaves = quad_tree::onTheFlyTraverse(srs::TileId{0, {0, 0}}, tile_scheduler::refineFunctor(camera, 4.0), [](const auto& v) { return srs::subtiles(v); });
  std::vector<srs::TileId> visible_leaves;
  visible_leaves.reserve(all_leaves.size());

  const auto is_visible = [&camera](const srs::TileId& tile) {
    const auto tile_aabb = srs::aabb(tile, 100, 4000);
    const auto triangles = geometry::clip(geometry::triangulise(tile_aabb), camera.clippingPlanes());
    if (triangles.empty())
      return false;
    return true;
  };

  std::copy_if(all_leaves.begin(), all_leaves.end(), std::back_inserter(visible_leaves), is_visible);
  return visible_leaves;
}

size_t SimplisticTileScheduler::numberOfTilesInTransit() const
{
  return m_pending_tile_requests.size();
}

size_t SimplisticTileScheduler::numberOfWaitingHeightTiles() const
{
  return m_received_height_tiles.size();
}

size_t SimplisticTileScheduler::numberOfWaitingOrthoTiles() const
{
  return m_received_ortho_tiles.size();
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
    if (m_unavaliable_tiles.contains(t))
      continue;
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
  m_received_ortho_tiles[tile_id] = data;
  checkLoadedTile(tile_id);
}

void SimplisticTileScheduler::receiveHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
  m_received_height_tiles[tile_id] = data;
  checkLoadedTile(tile_id);
}

void SimplisticTileScheduler::notifyAboutUnavailableOrthoTile(srs::TileId tile_id)
{
  m_unavaliable_tiles.insert(tile_id);
  m_pending_tile_requests.erase(tile_id);
  m_received_ortho_tiles.erase(tile_id);
  m_received_height_tiles.erase(tile_id);
}

void SimplisticTileScheduler::notifyAboutUnavailableHeightTile(srs::TileId tile_id)
{
  m_unavaliable_tiles.insert(tile_id);
  m_pending_tile_requests.erase(tile_id);
  m_received_ortho_tiles.erase(tile_id);
  m_received_height_tiles.erase(tile_id);
}

void SimplisticTileScheduler::checkLoadedTile(const srs::TileId& tile_id)
{
  if (m_received_height_tiles.contains(tile_id) &m_received_ortho_tiles.contains(tile_id)) {
    m_pending_tile_requests.erase(tile_id);
    auto heightraster = tile_conversion::qImage2uint16Raster(tile_conversion::toQImage(*m_received_height_tiles[tile_id]));
    auto ortho = tile_conversion::toQImage(*m_received_ortho_tiles[tile_id]);
    const auto tile = std::make_shared<Tile>(tile_id, srs::tile_bounds(tile_id), std::move(heightraster), std::move(ortho));
    m_received_ortho_tiles.erase(tile_id);
    m_received_height_tiles.erase(tile_id);
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

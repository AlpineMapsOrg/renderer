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

#include "alpine_renderer/tile_scheduler/BasicTreeTileScheduler.h"
#include "alpine_renderer/tile_scheduler/utils.h"
#include "alpine_renderer/utils/geometry.h"


BasicTreeTileScheduler::BasicTreeTileScheduler()
{
  m_root_node = std::make_unique<Node>(NodeData{.id = {0, {0, 0}}, .status = TileStatus::Uninitialised});
}


size_t BasicTreeTileScheduler::numberOfTilesInTransit() const
{
  unsigned counter = 0;
  quad_tree::visit(m_root_node.get(), [&counter](const NodeData& tile) { if (tile.status == TileStatus::InTransit) counter++; });
  return counter;
}

size_t BasicTreeTileScheduler::numberOfWaitingHeightTiles() const
{
  return {};
}

size_t BasicTreeTileScheduler::numberOfWaitingOrthoTiles() const
{
  return {};
}

TileScheduler::TileSet BasicTreeTileScheduler::gpuTiles() const
{
  // traverse tree and find all gpu nodes
  return {};
}

bool BasicTreeTileScheduler::enabled() const
{
  return m_enabled;
}

void BasicTreeTileScheduler::setEnabled(bool newEnabled)
{
}

void BasicTreeTileScheduler::updateCamera(const Camera& camera)
{
  if (!enabled())
    return;

  const auto refine_id = tile_scheduler::refineFunctor(camera, 1.0);
  const auto refine_data = [&](const auto& v) {
    return refine_id(v.id);
  };

  const auto generateChildren = [](const NodeData& v) {
    std::array<NodeData, 4> dta;
    const auto ids = srs::subtiles(v.id);
    for (unsigned i = 0; i < 4; ++i) {
      dta[i].id = ids[i];
    }
    return dta;
  };

  quad_tree::refine(m_root_node.get(), refine_data, generateChildren);

  const auto visitor = [&](NodeData& tile) {
    switch (tile.status) {
    case TileStatus::InTransit:
    case TileStatus::OnGpu:
    case TileStatus::Unavailable:
    case TileStatus::WaitingForSiblings:
      break;
    case TileStatus::Uninitialised:
      tile.status = TileStatus::InTransit;
      emit tileRequested(tile.id);
      break;
    }
  };
  quad_tree::visitChildren(m_root_node.get(), visitor);
}

void BasicTreeTileScheduler::receiveOrthoTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
}

void BasicTreeTileScheduler::receiveHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
}

void BasicTreeTileScheduler::checkLoadedTile(const srs::TileId& tile_id)
{

}

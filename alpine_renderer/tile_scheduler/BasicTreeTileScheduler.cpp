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
#include "alpine_renderer/utils/geometry.h"


BasicTreeTileScheduler::BasicTreeTileScheduler()
{
  m_root_node = std::make_unique<Node>(NodeData{.id = {0, {0, 0}}, .status = TileStatus::Uninitialised});
}


size_t BasicTreeTileScheduler::numberOfTilesInTransit() const
{
  // traverse tree and count
  return {};
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
  return {};
}

void BasicTreeTileScheduler::setEnabled(bool newEnabled)
{
}

void BasicTreeTileScheduler::updateCamera(const Camera& camera)
{
  if (!enabled())
    return;
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

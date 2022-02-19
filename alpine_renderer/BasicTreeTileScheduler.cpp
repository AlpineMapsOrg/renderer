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

#include "BasicTreeTileScheduler.h"

BasicTreeTileScheduler::BasicTreeTileScheduler()
{

}


size_t BasicTreeTileScheduler::numberOfTilesInTransit() const
{
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
}

void BasicTreeTileScheduler::receiveOrthoTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
}

void BasicTreeTileScheduler::receiveHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
}

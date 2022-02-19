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

#include <alpine_renderer/TileScheduler.h>
#include <QObject>

class BasicTreeTileScheduler : public TileScheduler
{
public:
  BasicTreeTileScheduler();

  size_t numberOfTilesInTransit() const override;
  size_t numberOfWaitingHeightTiles() const override;
  size_t numberOfWaitingOrthoTiles() const override;
  TileSet gpuTiles() const override;
  bool enabled() const override;
  void setEnabled(bool newEnabled) override;

public slots:
  void updateCamera(const Camera& camera) override;
  void loadOrthoTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data) override;
  void loadHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data) override;
};


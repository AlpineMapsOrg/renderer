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

#include <unordered_set>
#include <unordered_map>

#include <QObject>

#include "alpine_renderer/Camera.h"
#include "alpine_renderer/srs.h"

struct Tile;


class TileScheduler : public QObject
{
  Q_OBJECT
public:
  using TileSet = std::unordered_set<srs::TileId, srs::TileId::Hasher>;
  TileScheduler() = default;

  [[nodiscard]] virtual size_t numberOfTilesInTransit() const = 0;
  [[nodiscard]] virtual size_t numberOfWaitingHeightTiles() const = 0;
  [[nodiscard]] virtual size_t numberOfWaitingOrthoTiles() const = 0;
  [[nodiscard]] virtual TileSet gpuTiles() const = 0;

  virtual bool enabled() const = 0;
  virtual void setEnabled(bool newEnabled) = 0;

public slots:
  virtual void updateCamera(const Camera& camera) = 0;
  virtual void loadOrthoTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data) = 0;
  virtual void loadHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data) = 0;

signals:
  void tileRequested(const srs::TileId& tile_id);
  void tileReady(const std::shared_ptr<Tile>& tile);
  void tileExpired(const srs::TileId& tile_id);
};

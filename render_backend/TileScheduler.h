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

#include <QObject>

#include "render_backend/Camera.h"
#include "render_backend/srs.h"

struct Tile;

class TileScheduler : public QObject
{
  Q_OBJECT
public:
  TileScheduler();

  [[nodiscard]] std::vector<srs::TileId> loadCandidates(const Camera& camera) const;

public slots:
  void updateCamera(const Camera& camera);
  void loadOrthoTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data);
  void loadHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data);

signals:
  void tileRequested(const srs::TileId& tile_id);
  void tileReady(const std::shared_ptr<Tile>& tile);

private:
  void checkLoadedTile(const srs::TileId& tile_id);
  std::unordered_set<srs::TileId, srs::TileId::Hasher> m_pending_tile_requests;
  std::unordered_map<srs::TileId, std::shared_ptr<QByteArray>, srs::TileId::Hasher> m_loaded_ortho_tiles;
  std::unordered_map<srs::TileId, std::shared_ptr<QByteArray>, srs::TileId::Hasher> m_loaded_height_tiles;
};


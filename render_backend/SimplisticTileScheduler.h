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

#include "render_backend/TileScheduler.h"

class SimplisticTileScheduler : public TileScheduler
{
  Q_OBJECT
      public:
               SimplisticTileScheduler();

  [[nodiscard]] static std::vector<srs::TileId> loadCandidates(const Camera& camera) ;
  [[nodiscard]] size_t numberOfTilesInTransit() const override;
  [[nodiscard]] size_t numberOfWaitingHeightTiles() const override;
  [[nodiscard]] size_t numberOfWaitingOrthoTiles() const override;
  [[nodiscard]] TileSet gpuTiles() const override;

  bool enabled() const override;
  void setEnabled(bool newEnabled) override;

public slots:
  void updateCamera(const Camera& camera) override;
  void loadOrthoTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data) override;
  void loadHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data) override;

private:
  void checkLoadedTile(const srs::TileId& tile_id);
  TileSet m_pending_tile_requests;
  TileSet m_gpu_tiles;
  std::unordered_map<srs::TileId, std::shared_ptr<QByteArray>, srs::TileId::Hasher> m_loaded_ortho_tiles;
  std::unordered_map<srs::TileId, std::shared_ptr<QByteArray>, srs::TileId::Hasher> m_loaded_height_tiles;
  bool m_enabled = true;
};

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

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <QObject>

#include "nucleus/camera/Definition.h"
#include "sherpa/tile.h"


struct Tile;
namespace tile_scheduler {
class AabbDecorator;
using AabbDecoratorPtr = std::shared_ptr<AabbDecorator>;
}

class TileScheduler : public QObject {
    Q_OBJECT
public:
    using TileSet = std::unordered_set<tile::Id, tile::Id::Hasher>;
    using Tile2DataMap = std::unordered_map<tile::Id, std::shared_ptr<QByteArray>, tile::Id::Hasher>;
//    TileScheduler();
//    TileScheduler(const TileScheduler&) = delete;
//    TileScheduler(const TileScheduler&&) = delete;
//    ~TileScheduler() override;

//    void operator = (const TileScheduler&) = delete;
//    void operator = (const TileScheduler&&) = delete;

    [[nodiscard]] virtual size_t numberOfTilesInTransit() const = 0;
    [[nodiscard]] virtual size_t numberOfWaitingHeightTiles() const = 0;
    [[nodiscard]] virtual size_t numberOfWaitingOrthoTiles() const = 0;
    [[nodiscard]] virtual TileSet gpuTiles() const = 0;

    [[nodiscard]] virtual bool enabled() const = 0;
    virtual void setEnabled(bool newEnabled) = 0;

    [[nodiscard]] const tile_scheduler::AabbDecoratorPtr& aabb_decorator() const;
    void set_aabb_decorator(const tile_scheduler::AabbDecoratorPtr& new_aabb_decorator);

public slots:
    virtual void updateCamera(const camera::Definition& camera) = 0;
    virtual void receiveOrthoTile(tile::Id tile_id, std::shared_ptr<QByteArray> data) = 0;
    virtual void receiveHeightTile(tile::Id tile_id, std::shared_ptr<QByteArray> data) = 0;
    virtual void notifyAboutUnavailableOrthoTile(tile::Id tile_id) = 0;
    virtual void notifyAboutUnavailableHeightTile(tile::Id tile_id) = 0;

signals:
    void tileRequested(const tile::Id& tile_id);
    void tileReady(const std::shared_ptr<Tile>& tile);
    void tileExpired(const tile::Id& tile_id);
    void cancelTileRequest(const tile::Id& tile_id);
    void allTilesLoaded();

private:
    tile_scheduler::AabbDecoratorPtr m_aabb_decorator;
};

#ifndef SINGLETILELOADER_H
#define SINGLETILELOADER_H

#include "nucleus/TileScheduler.h"

/*
 * Loads one single tile and its neighbors
 */
class SingleTileLoader : public TileScheduler {
    Q_OBJECT

private:
    int radius = -1;

    Tile2DataMap m_received_ortho_tiles;
    Tile2DataMap m_received_height_tiles;

    TileSet m_pending_tile_requests;
    TileSet m_gpu_tiles;

    bool m_enabled = false;

public:
    //radius defines how many neighbors shal be loaded additionally
    //radius of 1 means one to the left right top and botton -> essentially the 3x3 neighborhood
    SingleTileLoader(int radius);

    void loadTiles(const tile::Id& center_tile);

    [[nodiscard]] virtual size_t numberOfTilesInTransit() const override;
    [[nodiscard]] virtual size_t numberOfWaitingHeightTiles() const override;
    [[nodiscard]] virtual size_t numberOfWaitingOrthoTiles() const override;
    [[nodiscard]] virtual TileSet gpuTiles() const override;

    [[nodiscard]] virtual bool enabled() const override;
    virtual void setEnabled(bool newEnabled) override;

public slots:
    virtual void updateCamera(const camera::Definition& camera) override;

    virtual void receiveOrthoTile(tile::Id tile_id, std::shared_ptr<QByteArray> data) override;
    virtual void receiveHeightTile(tile::Id tile_id, std::shared_ptr<QByteArray> data) override;
    virtual void notifyAboutUnavailableOrthoTile(tile::Id tile_id) override;
    virtual void notifyAboutUnavailableHeightTile(tile::Id tile_id) override;

private:
    void checkLoadedTile(const tile::Id& tile_id);
    void requestTile(const tile::Id& tile_id);
    void removeTile(const tile::Id& tile_id);
    void loadTiles();
};

#endif // SINGLETILELOADER_H

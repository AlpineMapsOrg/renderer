#include "nucleus/tile_scheduler/SingleTileLoader.h"
#include "nucleus/tile_scheduler/utils.h"

#include "nucleus/Tile.h"
#include "nucleus/utils/QuadTree.h"
#include "nucleus/utils/tile_conversion.h"
#include "sherpa/geometry.h"

#include <QDebug>


SingleTileLoader::SingleTileLoader(int radius):
    radius(radius)
{

}

void SingleTileLoader::requestTile(const tile::Id& tile_id) {
    if (!m_gpu_tiles.contains(tile_id)) {
        m_pending_tile_requests.emplace(tile_id);

        emit tileRequested(tile_id);
    }
}

void SingleTileLoader::removeTile(const tile::Id& tile_id) {
    emit tileExpired(tile_id);
    m_gpu_tiles.erase(tile_id);
}

void SingleTileLoader::loadTiles(const tile::Id& center_tile) {
    m_received_height_tiles.clear();
    m_received_ortho_tiles.clear();

    TileSet requested_tiles;

    for (int offset_x = -radius; offset_x <= radius; offset_x++) {
        for (int offset_y = -radius; offset_y <= radius; offset_y++) {
            tile::Id neighbor_tile = {center_tile.zoom_level, center_tile.coords, center_tile.scheme};

            neighbor_tile.coords += glm::ivec2(offset_x, offset_y);

            requestTile(neighbor_tile);

            requested_tiles.emplace(neighbor_tile);
        }
    }

    TileSet tiles_to_remove;
    for(const auto& tile_id : m_gpu_tiles) {
        if (!requested_tiles.contains(tile_id)) {
            tiles_to_remove.emplace(tile_id);
        }
    }

    std::for_each(tiles_to_remove.begin(), tiles_to_remove.end(), [this](const tile::Id& tile_id) { removeTile(tile_id); });
}

void SingleTileLoader::checkLoadedTile(const tile::Id& tile_id)
{
    if (m_received_height_tiles.contains(tile_id) && m_received_ortho_tiles.contains(tile_id)) {
        m_pending_tile_requests.erase(tile_id);

        auto heightraster = tile_conversion::qImage2uint16Raster(tile_conversion::toQImage(*m_received_height_tiles[tile_id]));
        auto ortho = tile_conversion::toQImage(*m_received_ortho_tiles[tile_id]);
        const auto tile = std::make_shared<Tile>(tile_id, this->aabb_decorator()->aabb(tile_id), std::move(heightraster), std::move(ortho));

        m_received_ortho_tiles.erase(tile_id);
        m_received_height_tiles.erase(tile_id);

        m_gpu_tiles.emplace(tile_id);

        emit tileReady(tile);
    }

    if (m_pending_tile_requests.empty()) {
        emit allTilesLoaded();
    }
}

void SingleTileLoader::updateCamera(const camera::Definition& camera) {

}

void SingleTileLoader::receiveOrthoTile(tile::Id tile_id, std::shared_ptr<QByteArray> data)
{
    m_received_ortho_tiles.emplace(tile_id, data);
    checkLoadedTile(tile_id);
}

void SingleTileLoader::receiveHeightTile(tile::Id tile_id, std::shared_ptr<QByteArray> data)
{
    m_received_height_tiles.emplace(tile_id, data);
    checkLoadedTile(tile_id);
}


void SingleTileLoader::notifyAboutUnavailableOrthoTile(tile::Id tile_id)
{
    m_pending_tile_requests.erase(tile_id);
    qDebug() << "[" << tile_id.coords.x << "|" << tile_id.coords.y << "]" << " not available!";
    if (m_pending_tile_requests.empty()) {
        emit allTilesLoaded();
    }
}

void SingleTileLoader::notifyAboutUnavailableHeightTile(tile::Id tile_id)
{
    notifyAboutUnavailableOrthoTile(tile_id);
}

size_t SingleTileLoader::numberOfTilesInTransit() const {
    return m_received_height_tiles.size() + m_received_ortho_tiles.size();
}
size_t SingleTileLoader::numberOfWaitingHeightTiles() const {
    return m_pending_tile_requests.size();
}
size_t SingleTileLoader::numberOfWaitingOrthoTiles() const {
    return m_pending_tile_requests.size();
}
TileScheduler::TileSet SingleTileLoader::gpuTiles() const {
    return m_gpu_tiles;
}

bool SingleTileLoader::enabled() const {
    return m_enabled;
}

void SingleTileLoader::setEnabled(bool newEnabled) {
   m_enabled = newEnabled;
};

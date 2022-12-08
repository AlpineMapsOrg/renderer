/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek
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

#include "GpuCacheTileScheduler.h"
#include <QBuffer>
#include <unordered_set>

#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/Tile.h"
#include "nucleus/srs.h"
#include "nucleus/utils/QuadTree.h"
#include "nucleus/utils/tile_conversion.h"
#include "sherpa/geometry.h"
#include "sherpa/hasher.h"
#include "sherpa/iterator.h"


GpuCacheTileScheduler::GpuCacheTileScheduler() = default;

TileScheduler::TileSet GpuCacheTileScheduler::loadCandidates(const camera::Definition& camera, const tile_scheduler::AabbDecoratorPtr& aabb_decorator)
{
    //  return quad_tree::onTheFlyTraverse(tile::Id{0, {0, 0}}, tile_scheduler::refineFunctor(camera, 1.0), [](const auto& v) { return srs::subtiles(v); });
    std::unordered_set<tile::Id, tile::Id::Hasher> all_tiles;
    const auto all_leaves = quad_tree::onTheFlyTraverse(
        tile::Id { 0, { 0, 0 } },
        tile_scheduler::refineFunctor(camera, aabb_decorator, 2.0),
        [&all_tiles](const tile::Id& v) { all_tiles.insert(v); return v.children(); });
    std::vector<tile::Id> visible_leaves;
    visible_leaves.reserve(all_leaves.size());

    all_tiles.reserve(all_tiles.size() + all_leaves.size());
    std::copy(all_leaves.begin(), all_leaves.end(), sherpa::unordered_inserter(all_tiles));


//    const auto is_visible = [&camera, aabb_decorator](const tile::Id& tile) {
//        return tile_scheduler::cameraFrustumContainsTile(camera, aabb_decorator->aabb(tile));
//    };

//    std::copy_if(all_leaves.begin(), all_leaves.end(), std::back_inserter(visible_leaves), is_visible);
    return all_tiles;
}

size_t GpuCacheTileScheduler::numberOfTilesInTransit() const
{
    return m_pending_tile_requests.size();
}

size_t GpuCacheTileScheduler::numberOfWaitingHeightTiles() const
{
    return m_received_height_tiles.size();
}

size_t GpuCacheTileScheduler::numberOfWaitingOrthoTiles() const
{
    return m_received_ortho_tiles.size();
}

TileScheduler::TileSet GpuCacheTileScheduler::gpuTiles() const
{
    return m_gpu_tiles;
}

void GpuCacheTileScheduler::updateCamera(const camera::Definition& camera)
{
    if (!enabled())
        return;

    m_current_camera = camera;
    const auto aabb_decorator = this->aabb_decorator();

//    const auto outside_camera_frustum = [&camera, aabb_decorator](const auto& gpu_tile_id) {
//        return !tile_scheduler::cameraFrustumContainsTile(camera, aabb_decorator->aabb(gpu_tile_id));
//    };
//    removeGpuTileIf(outside_camera_frustum);

    const auto tiles = loadCandidates(camera, aabb_decorator);
    for (const auto& t : tiles) {
        if (m_pending_tile_requests.contains(t)) // todo cancel current requests
            continue;
        if (m_gpu_tiles.contains(t)) {
            continue;
        }
        m_pending_tile_requests.insert(t);
        emit tileRequested(t);
    }
}

void GpuCacheTileScheduler::receiveOrthoTile(tile::Id tile_id, std::shared_ptr<QByteArray> data)
{
    m_received_ortho_tiles[tile_id] = data;
    checkLoadedTile(tile_id);
}

void GpuCacheTileScheduler::receiveHeightTile(tile::Id tile_id, std::shared_ptr<QByteArray> data)
{
    m_received_height_tiles[tile_id] = data;
    checkLoadedTile(tile_id);
}

void GpuCacheTileScheduler::notifyAboutUnavailableOrthoTile(tile::Id tile_id)
{
    QImage default_tile(m_ortho_tile_size, m_ortho_tile_size, QImage::Format_ARGB32);
    default_tile.fill(Qt::GlobalColor::white);
    QByteArray arr;
    QBuffer buffer(&arr);
    buffer.open(QIODevice::WriteOnly);
    default_tile.save(&buffer, "JPEG");
    receiveOrthoTile(tile_id, std::make_shared<QByteArray>(arr));
}

void GpuCacheTileScheduler::notifyAboutUnavailableHeightTile(tile::Id tile_id)
{
    QImage default_tile(m_ortho_tile_size, m_ortho_tile_size, QImage::Format_ARGB32);
    default_tile.fill(Qt::GlobalColor::black);
    QByteArray arr;
    QBuffer buffer(&arr);
    buffer.open(QIODevice::WriteOnly);
    default_tile.save(&buffer, "PNG");
    receiveHeightTile(tile_id, std::make_shared<QByteArray>(arr));
}

void GpuCacheTileScheduler::set_tile_cache_size(unsigned tile_cache_size)
{
    m_tile_cache_size = tile_cache_size;
}

void GpuCacheTileScheduler::purge_cache_from_old_tiles()
{
    if (m_gpu_tiles.size() <= m_tile_cache_size)
        return;

    const auto necessary_tiles = loadCandidates(m_current_camera, this->aabb_decorator());

    std::vector<tile::Id> unnecessary_tiles;
    unnecessary_tiles.reserve(m_gpu_tiles.size());
    std::copy_if(m_gpu_tiles.cbegin(), m_gpu_tiles.cend(), std::back_inserter(unnecessary_tiles), [&necessary_tiles](const auto& tile_id) { return !necessary_tiles.contains(tile_id); });

    const auto n_tiles_to_be_removed = m_gpu_tiles.size() - m_tile_cache_size;
    if (n_tiles_to_be_removed >= unnecessary_tiles.size()) {
        remove_gpu_tiles(unnecessary_tiles); // cache too small. can't remove 'enough', so remove everything we can
        return;
    }
    const auto last_remove_tile_iter = unnecessary_tiles.begin() + n_tiles_to_be_removed;
    assert(last_remove_tile_iter < unnecessary_tiles.end());

    std::nth_element(unnecessary_tiles.begin(), last_remove_tile_iter, unnecessary_tiles.end(), [](const tile::Id& a, const tile::Id& b) {
        return !(a < b);
    });
    unnecessary_tiles.resize(n_tiles_to_be_removed);
    remove_gpu_tiles(unnecessary_tiles);
}

void GpuCacheTileScheduler::checkLoadedTile(const tile::Id& tile_id)
{
    if (m_received_height_tiles.contains(tile_id) && m_received_ortho_tiles.contains(tile_id)) {
        m_pending_tile_requests.erase(tile_id);
        auto heightraster = tile_conversion::qImage2uint16Raster(tile_conversion::toQImage(*m_received_height_tiles[tile_id]));
        auto ortho = tile_conversion::toQImage(*m_received_ortho_tiles[tile_id]);
        const auto tile = std::make_shared<Tile>(tile_id, this->aabb_decorator()->aabb(tile_id), std::move(heightraster), std::move(ortho));
        m_received_ortho_tiles.erase(tile_id);
        m_received_height_tiles.erase(tile_id);

//        const auto overlaps = [&tile_id](const auto& gpu_tile_id) { return srs::overlap(gpu_tile_id, tile_id); };
//        removeGpuTileIf(overlaps);

        m_gpu_tiles.insert(tile_id);
        emit tileReady(tile);
        purge_cache_from_old_tiles();
    }
}

bool GpuCacheTileScheduler::enabled() const
{
    return m_enabled;
}

void GpuCacheTileScheduler::setEnabled(bool newEnabled)
{
    m_enabled = newEnabled;
}

void GpuCacheTileScheduler::remove_gpu_tiles(const std::vector<tile::Id>& tiles)
{
    for (const auto& id : tiles) {
        emit tileExpired(id);
        m_gpu_tiles.erase(id);
    }
}

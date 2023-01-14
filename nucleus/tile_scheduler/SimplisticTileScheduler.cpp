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

#include "nucleus/tile_scheduler/SimplisticTileScheduler.h"

#include "nucleus/Tile.h"
#include "nucleus/srs.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/tile_conversion.h"
#include "sherpa/geometry.h"
#include "sherpa/quad_tree.h"

SimplisticTileScheduler::SimplisticTileScheduler() = default;

std::vector<tile::Id> SimplisticTileScheduler::load_candidates(const camera::Definition& camera, const tile_scheduler::AabbDecoratorPtr& aabb_decorator)
{
    //  return quad_tree::onTheFlyTraverse(tile::Id{0, {0, 0}}, tile_scheduler::refineFunctor(camera, 1.0), [](const auto& v) { return srs::subtiles(v); });
    const auto all_leaves = quad_tree::onTheFlyTraverse(tile::Id { 0, { 0, 0 } }, tile_scheduler::refineFunctor(camera, aabb_decorator, 2.0), [](const tile::Id& v) { return v.children(); });
    std::vector<tile::Id> visible_leaves;
    visible_leaves.reserve(all_leaves.size());

    const auto is_visible = [&camera, aabb_decorator](const tile::Id& tile) {
        return tile_scheduler::cameraFrustumContainsTile(camera, aabb_decorator->aabb(tile));
    };

    std::copy_if(all_leaves.begin(), all_leaves.end(), std::back_inserter(visible_leaves), is_visible);
    return visible_leaves;
}

size_t SimplisticTileScheduler::number_of_tiles_in_transit() const
{
    return m_pending_tile_requests.size();
}

size_t SimplisticTileScheduler::number_of_waiting_height_tiles() const
{
    return m_received_height_tiles.size();
}

size_t SimplisticTileScheduler::number_of_waiting_ortho_tiles() const
{
    return m_received_ortho_tiles.size();
}

SimplisticTileScheduler::TileSet SimplisticTileScheduler::gpu_tiles() const
{
    return m_gpu_tiles;
}

void SimplisticTileScheduler::update_camera(const camera::Definition& camera)
{
    if (!enabled())
        return;

    const auto aabb_decorator = this->aabb_decorator();

    const auto outside_camera_frustum = [&camera, aabb_decorator](const auto& gpu_tile_id) {
        return !tile_scheduler::cameraFrustumContainsTile(camera, aabb_decorator->aabb(gpu_tile_id));
    };
    removeGpuTileIf(outside_camera_frustum);

    const auto tiles = load_candidates(camera, aabb_decorator);
    for (const auto& t : tiles) {
        if (m_unavaliable_tiles.contains(t))
            continue;
        if (m_pending_tile_requests.contains(t)) // todo cancel current requests
            continue;
        if (m_gpu_tiles.contains(t)) {
            continue;
        }
        m_pending_tile_requests.insert(t);
        emit tile_requested(t);
    }
}

void SimplisticTileScheduler::receive_ortho_tile(tile::Id tile_id, std::shared_ptr<QByteArray> data)
{
    if (m_unavaliable_tiles.contains(tile_id))
        return;
    m_received_ortho_tiles[tile_id] = data;
    checkLoadedTile(tile_id);
}

void SimplisticTileScheduler::receive_height_tile(tile::Id tile_id, std::shared_ptr<QByteArray> data)
{
    if (m_unavaliable_tiles.contains(tile_id))
        return;
    m_received_height_tiles[tile_id] = data;
    checkLoadedTile(tile_id);
}

void SimplisticTileScheduler::notify_about_unavailable_ortho_tile(tile::Id tile_id)
{
    m_unavaliable_tiles.insert(tile_id);
    m_pending_tile_requests.erase(tile_id);
    m_received_ortho_tiles.erase(tile_id);
    m_received_height_tiles.erase(tile_id);
}

void SimplisticTileScheduler::notify_about_unavailable_height_tile(tile::Id tile_id)
{
    m_unavaliable_tiles.insert(tile_id);
    m_pending_tile_requests.erase(tile_id);
    m_received_ortho_tiles.erase(tile_id);
    m_received_height_tiles.erase(tile_id);
}

void SimplisticTileScheduler::checkLoadedTile(const tile::Id& tile_id)
{
    if (m_received_height_tiles.contains(tile_id) && m_received_ortho_tiles.contains(tile_id)) {
        m_pending_tile_requests.erase(tile_id);
        auto heightraster = tile_conversion::qImage2uint16Raster(tile_conversion::toQImage(*m_received_height_tiles[tile_id]));
        auto ortho = tile_conversion::toQImage(*m_received_ortho_tiles[tile_id]);
        const auto tile = std::make_shared<Tile>(tile_id, this->aabb_decorator()->aabb(tile_id), std::move(heightraster), std::move(ortho));
        m_received_ortho_tiles.erase(tile_id);
        m_received_height_tiles.erase(tile_id);

        const auto overlaps = [&tile_id](const auto& gpu_tile_id) { return srs::overlap(gpu_tile_id, tile_id); };
        removeGpuTileIf(overlaps);

        m_gpu_tiles.insert(tile_id);
        emit tile_ready(tile);
    }
}

bool SimplisticTileScheduler::enabled() const
{
    return m_enabled;
}

void SimplisticTileScheduler::set_enabled(bool newEnabled)
{
    m_enabled = newEnabled;
}

template <typename Predicate>
void SimplisticTileScheduler::removeGpuTileIf(Predicate condition)
{
    std::vector<tile::Id> overlapping_tiles;
    overlapping_tiles.reserve(4);
    std::copy_if(m_gpu_tiles.cbegin(), m_gpu_tiles.cend(), std::back_inserter(overlapping_tiles), condition);
    for (const auto& gpu_tile_id : overlapping_tiles) {
        emit tile_expired(gpu_tile_id);
        m_gpu_tiles.erase(gpu_tile_id);
    }
}

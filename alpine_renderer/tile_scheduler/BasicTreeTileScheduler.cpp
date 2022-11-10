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

#include "alpine_renderer/tile_scheduler/BasicTreeTileScheduler.h"
#include "alpine_renderer/Tile.h"
#include "alpine_renderer/tile_scheduler/utils.h"
#include "alpine_renderer/utils/geometry.h"
#include "alpine_renderer/utils/tile_conversion.h"

BasicTreeTileScheduler::BasicTreeTileScheduler()
{
    m_root_node = std::make_unique<Node>(NodeData { .id = { 0, { 0, 0 } }, .status = TileStatus::Uninitialised });
}

size_t BasicTreeTileScheduler::numberOfTilesInTransit() const
{
    unsigned counter = 0;
    quad_tree::visit(m_root_node.get(), [&counter](const NodeData& tile) { if (tile.status == TileStatus::InTransit) counter++; });
    return counter;
}

size_t BasicTreeTileScheduler::numberOfWaitingHeightTiles() const
{
    return m_received_height_tiles.size();
}

size_t BasicTreeTileScheduler::numberOfWaitingOrthoTiles() const
{
    return m_received_ortho_tiles.size();
}

TileScheduler::TileSet BasicTreeTileScheduler::gpuTiles() const
{
    // traverse tree and find all gpu nodes
    TileScheduler::TileSet gpu_tiles;
    const auto visitor = [&](const NodeData& tile) {
        switch (tile.status) {
        case TileStatus::InTransit:
        case TileStatus::Unavailable:
        case TileStatus::WaitingForSiblings:
        case TileStatus::Uninitialised:
            break;
        case TileStatus::OnGpu:
            gpu_tiles.insert(tile.id);
            break;
        }
    };
    quad_tree::visit(m_root_node.get(), visitor);
    return gpu_tiles;
}

bool BasicTreeTileScheduler::enabled() const
{
    return m_enabled;
}

void BasicTreeTileScheduler::setEnabled(bool newEnabled)
{
    m_enabled = newEnabled;
}

void BasicTreeTileScheduler::updateCamera(const Camera& camera)
{
    if (!enabled())
        return;

    { // reduce tree
        const auto refine_id = tile_scheduler::refineFunctor(camera, 0.5);
        const auto clean_up = [&](const NodeData& v) {
            if (refine_id(v.id))
                return;
            switch (v.status) {
            case TileStatus::Unavailable:
            case TileStatus::WaitingForSiblings:
            case TileStatus::Uninitialised:
                break;
            case TileStatus::InTransit:
                // todo: cancel request and make sure it doesn't end up in the received tiles.
                // see also todo in checkLoadedTile / tile shipping
                break;
            case TileStatus::OnGpu:
                m_gpu_tiles_to_be_expired.insert(v.id);
                break;
            }
        };
        quad_tree::visit(m_root_node.get(), clean_up);

        const auto refine_data = [&](const NodeData& v) {
            return refine_id(v.id);
        };
        quad_tree::reduce(m_root_node.get(), refine_data);
    }

    { // refine tree
        const auto refine_id = tile_scheduler::refineFunctor(camera, 1.0);
        const auto refine_data = [&](const auto& v) {
            return refine_id(v.id);
        };

        const auto generateChildren = [](const NodeData& v) {
            std::array<NodeData, 4> dta;
            const auto ids = srs::subtiles(v.id);
            for (unsigned i = 0; i < 4; ++i) {
                dta[i].id = ids[i];
            }
            return dta;
        };
        quad_tree::refine(m_root_node.get(), refine_data, generateChildren);
    }

    { // emit tile requests
        std::vector<srs::TileId> tile_requests;
        const auto visitor = [&](NodeData& tile) {
            switch (tile.status) {
            case TileStatus::InTransit:
            case TileStatus::OnGpu:
            case TileStatus::Unavailable:
            case TileStatus::WaitingForSiblings:
                break;
            case TileStatus::Uninitialised:
                tile.status = TileStatus::InTransit;
                tile_requests.push_back(tile.id);
                break;
            }
        };
        quad_tree::visitLeaves(m_root_node.get(), visitor);

        // do not interleave tree traversal and signal emits
        // 1. when single threaded, the signals are emitted synchronously, and the tree needs to be in a consistent state for the slots in this implementation
        // 2. it's likely also better for performance, as emitting a signal can be a lot of function calls. this should (tm) be better for locality.
        for (const auto& id : tile_requests)
            emit tileRequested(id);
    }
}

void BasicTreeTileScheduler::receiveOrthoTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
    assert(data);
    m_received_ortho_tiles[tile_id] = data;
    checkLoadedTile(tile_id); // should go on a qtimer or something, so that the expensive checkLoadTile is not called too often, similar to qwidget update()
}

void BasicTreeTileScheduler::receiveHeightTile(srs::TileId tile_id, std::shared_ptr<QByteArray> data)
{
    assert(data);
    m_received_height_tiles[tile_id] = data;
    checkLoadedTile(tile_id);
}

void BasicTreeTileScheduler::notifyAboutUnavailableOrthoTile(srs::TileId tile_id)
{
    markTileUnavailable(tile_id);
}

void BasicTreeTileScheduler::notifyAboutUnavailableHeightTile(srs::TileId tile_id)
{
    markTileUnavailable(tile_id);
}

void BasicTreeTileScheduler::checkConsistency() const
{
#ifndef NDEBUG
    {
        bool no_leaf_is_Uninitialised = true;
        // setting received tiles to waiting
        const auto visitor = [&](const NodeData& tile) {
            switch (tile.status) {
            case TileStatus::InTransit:
            case TileStatus::OnGpu:
            case TileStatus::Unavailable:
            case TileStatus::WaitingForSiblings:
                break;
            case TileStatus::Uninitialised:
                no_leaf_is_Uninitialised = false;
            }
        };
        quad_tree::visitLeaves(m_root_node.get(), visitor);
        assert(no_leaf_is_Uninitialised);
    }
    {
        bool no_inner_node_is_on_the_gpu = true;
        // setting received tiles to waiting
        const auto visitor = [&](const NodeData& tile) {
            switch (tile.status) {
            case TileStatus::InTransit:
            case TileStatus::Unavailable:
            case TileStatus::WaitingForSiblings:
            case TileStatus::Uninitialised:
                break;
            case TileStatus::OnGpu:
                no_inner_node_is_on_the_gpu = false;
            }
        };
        quad_tree::visitInnerNodes(m_root_node.get(), visitor);
        assert(no_inner_node_is_on_the_gpu);
    }
#endif
}

void BasicTreeTileScheduler::checkLoadedTile(const srs::TileId&)
{
    // setting received tiles to waiting and check if we are ready to ship
    auto ready_to_ship = true;
    {
        const auto visitor = [&](NodeData& tile) {
            switch (tile.status) {
            case TileStatus::InTransit:
                if (m_received_height_tiles.contains(tile.id) && m_received_ortho_tiles.contains(tile.id))
                    tile.status = TileStatus::WaitingForSiblings;
                else
                    ready_to_ship = false;
                break;
            case TileStatus::OnGpu:
            case TileStatus::Unavailable:
            case TileStatus::WaitingForSiblings:
                break;
            case TileStatus::Uninitialised:
                assert(false);
                break;
            }
        };
        quad_tree::visitLeaves(m_root_node.get(), visitor);
    }

    // ship
    if (ready_to_ship) {
        std::vector<srs::TileId> tile_expiries;
        for (const auto& id : m_gpu_tiles_to_be_expired)
            tile_expiries.push_back(id);
        m_gpu_tiles_to_be_expired = {};
        std::vector<std::shared_ptr<Tile>> tiles_ready;

        { // remove old tiles

            const auto visitor = [&](NodeData& tile) {
                switch (tile.status) {
                case TileStatus::InTransit:
                case TileStatus::WaitingForSiblings:
                    // todo: temporary, we are not cancelling yet
                    //          assert(false);
                    break;
                case TileStatus::Uninitialised:
                case TileStatus::Unavailable:
                    break;
                case TileStatus::OnGpu:
                    tile.status = TileStatus::Uninitialised;
                    tile_expiries.push_back(tile.id);
                    break;
                }
            };
            quad_tree::visitInnerNodes(m_root_node.get(), visitor);
        }
        { // add new tiles
            const auto visitor = [&](NodeData& tile) {
                switch (tile.status) {
                case TileStatus::InTransit:
                case TileStatus::Uninitialised:
                    assert(false);
                    break;
                case TileStatus::OnGpu:
                case TileStatus::Unavailable:
                    break;
                case TileStatus::WaitingForSiblings:
                    tile.status = TileStatus::OnGpu;
                    assert(m_received_height_tiles.contains(tile.id));
                    assert(m_received_ortho_tiles.contains(tile.id));
                    auto heightraster = tile_conversion::qImage2uint16Raster(tile_conversion::toQImage(*m_received_height_tiles[tile.id]));
                    auto ortho = tile_conversion::toQImage(*m_received_ortho_tiles[tile.id]);
                    const auto new_tile = std::make_shared<Tile>(tile.id, srs::tile_bounds(tile.id), std::move(heightraster), std::move(ortho));
                    m_received_ortho_tiles.erase(tile.id);
                    m_received_height_tiles.erase(tile.id);
                    tiles_ready.push_back(new_tile);

                    break;
                }
            };
            quad_tree::visitLeaves(m_root_node.get(), visitor);
        }
#ifndef NDEBUG
        checkConsistency();
#endif

        // do not interleave tree traversal and signal emits
        // 1. when single threaded, the signals are emitted synchronously, and the tree needs to be in a consistent state for the slots in this implementation
        // 2. it's likely also better for performance, as emitting a signal can be a lot of function calls. this should (tm) be better for locality.
        for (const auto& id : tile_expiries)
            emit tileExpired(id);

        for (const auto& tile : tiles_ready)
            emit tileReady(tile);
    }
}

void BasicTreeTileScheduler::markTileUnavailable(const srs::TileId& unavailable_tile_id)
{
    const auto visitor = [&](NodeData& tile) {
        if (tile.id != unavailable_tile_id)
            return;
        switch (tile.status) {
        case TileStatus::InTransit:
            tile.status = TileStatus::Unavailable;
            break;
        case TileStatus::Uninitialised:
        case TileStatus::Unavailable:
            break;
        case TileStatus::OnGpu:
        case TileStatus::WaitingForSiblings:
            assert(false);
            break;
        }
    };
    quad_tree::visit(m_root_node.get(), visitor);
}

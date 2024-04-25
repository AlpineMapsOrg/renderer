/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "compute.h"

namespace webgpu_engine {

std::vector<tile::Id> RectangularTileRegion::get_tiles() const
{
    assert(min.x <= max.x);
    assert(min.y <= max.y);
    std::vector<tile::Id> tiles;
    tiles.reserve((min.x - max.x + 1) * (min.y - max.y + 1));
    for (unsigned x = min.x; x <= max.x; x++) {
        for (unsigned y = min.y; y <= max.y; x++) {
            tiles.emplace_back(tile::Id { zoom_level, { x, y }, scheme });
        }
    }
    return tiles;
}

TextureArrayComputeTileStorage::TextureArrayComputeTileStorage(size_t n_edge_vertices, size_t capacity)
    : m_n_edge_vertices { n_edge_vertices }
    , m_capacity { m_capacity }
{
}

void TextureArrayComputeTileStorage::init() { }

void TextureArrayComputeTileStorage::store([[maybe_unused]] const tile::Id& id, [[maybe_unused]] std::shared_ptr<QByteArray> data) { }

void TextureArrayComputeTileStorage::clear([[maybe_unused]] const tile::Id& id) { }

ComputeController::ComputeController()
    : m_tile_loader { std::make_unique<nucleus::tile_scheduler::TileLoadService>(
        "https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY, ".png") }
    , m_tile_storage { std::make_unique<TextureArrayComputeTileStorage>(m_num_edge_vertices, m_max_num_tiles) }
{
    connect(m_tile_loader.get(), &nucleus::tile_scheduler::TileLoadService::load_finished, this, &ComputeController::on_single_tile_received);
}

void ComputeController::request_and_store_tiles(const RectangularTileRegion& region)
{
    std::vector<tile::Id> tiles_in_region = region.get_tiles();
    assert(tiles_in_region.size() <= m_max_num_tiles);
    m_num_tiles_requested = tiles_in_region.size();
    m_num_tiles_received = 0;
    for (const auto& tile : tiles_in_region) {
        m_tile_loader->load(tile);
    }
}

void ComputeController::on_single_tile_received(const nucleus::tile_scheduler::tile_types::TileLayer& tile)
{
    m_tile_storage->store(tile.id, tile.data);
    m_num_tiles_received++;
    if (m_num_tiles_received == m_num_tiles_requested) {
        on_all_tiles_received();
    }
}

void ComputeController::on_all_tiles_received() { }

} // namespace webgpu_engine

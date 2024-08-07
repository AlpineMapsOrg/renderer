/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include "LayerAssembler.h"

using namespace nucleus::tile_scheduler;

LayerAssembler::LayerAssembler(QObject* parent)
    : QObject { parent }
{
}

size_t LayerAssembler::n_items_in_flight() const { return m_height_data.size() + m_ortho_data.size() + m_vector_tile_data.size(); }

tile_types::LayeredTile LayerAssembler::join(
    const tile_types::TileLayer& ortho_tile, const tile_types::TileLayer& height_tile, const tile_types::TileLayer& vector_tile)
{
    assert(ortho_tile.id == height_tile.id);
    assert(ortho_tile.id == vector_tile.id);
    const auto network_info
        = tile_types::NetworkInfo::join(ortho_tile.network_info, height_tile.network_info); //, vector_tile.network_info -> vector tile might be 404 if empty
    const auto data_filter = [&network_info](const auto& d) {
        if (network_info.status == tile_types::NetworkInfo::Status::Good)
            return d;
        return std::make_shared<QByteArray>();
    };

    return { ortho_tile.id, network_info, data_filter(ortho_tile.data), data_filter(height_tile.data), data_filter(vector_tile.data) };
}

void LayerAssembler::load(const tile::Id& tile_id)
{
    emit tile_requested(tile_id);
}

void LayerAssembler::deliver_ortho(const tile_types::TileLayer& tile)
{
    m_ortho_data[tile.id] = tile;
    check_and_emit(tile.id);
}

void LayerAssembler::deliver_height(const tile_types::TileLayer& tile)
{
    m_height_data[tile.id] = tile;
    check_and_emit(tile.id);
}

void LayerAssembler::deliver_vectortile(const tile_types::TileLayer& tile)
{
    m_vector_tile_data[tile.id] = tile;
    check_and_emit(tile.id);
}

void LayerAssembler::check_and_emit(const tile::Id& tile_id)
{
    if (m_ortho_data.contains(tile_id) && m_height_data.contains(tile_id) && m_vector_tile_data.contains(tile_id)) {
        emit tile_loaded(join(m_ortho_data[tile_id], m_height_data[tile_id], m_vector_tile_data[tile_id]));
        m_ortho_data.erase(tile_id);
        m_height_data.erase(tile_id);
        m_vector_tile_data.erase(tile_id);
    }
}

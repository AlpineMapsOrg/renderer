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

size_t LayerAssembler::n_items_in_flight() const
{
    return m_height_data.size() + m_ortho_data.size();
}

void LayerAssembler::load(const tile::Id& tile_id)
{
    emit tile_requested(tile_id);
}

void LayerAssembler::deliver_ortho(const tile::Id& tile_id, const std::shared_ptr<const QByteArray>& ortho_data)
{
    m_ortho_data[tile_id] = ortho_data;
    check_and_emit(tile_id);
}

void LayerAssembler::deliver_height(const tile::Id& tile_id, const std::shared_ptr<const QByteArray>& height_data)
{
    m_height_data[tile_id] = height_data;
    check_and_emit(tile_id);
}

void LayerAssembler::report_missing_ortho(const tile::Id& tile_id)
{
    m_ortho_data[tile_id] = {};
    check_and_emit(tile_id);
}

void LayerAssembler::report_missing_height(const tile::Id& tile_id)
{
    m_height_data[tile_id] = {};
    check_and_emit(tile_id);
}

void LayerAssembler::check_and_emit(const tile::Id& tile_id)
{
    if (m_ortho_data.contains(tile_id) && m_height_data.contains(tile_id)) {
        emit tile_loaded({ .id = tile_id, .ortho = m_ortho_data[tile_id], .height = m_height_data[tile_id] });
        m_ortho_data.erase(tile_id);
        m_height_data.erase(tile_id);
    }
}

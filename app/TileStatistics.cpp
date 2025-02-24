/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adam Celarek
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

#include "TileStatistics.h"

TileStatistics::TileStatistics(QObject* parent)
    : QObject { parent }
{
}

unsigned int TileStatistics::n_label_tiles_gpu() const { return m_n_label_tiles_gpu; }

void TileStatistics::set_n_label_tiles_gpu(unsigned int new_n_label_tiles_gpu)
{
    if (m_n_label_tiles_gpu == new_n_label_tiles_gpu)
        return;
    m_n_label_tiles_gpu = new_n_label_tiles_gpu;
    emit n_label_tiles_gpu_changed(m_n_label_tiles_gpu);
}

unsigned int TileStatistics::n_label_tiles_drawn() const { return m_n_label_tiles_drawn; }

void TileStatistics::set_n_label_tiles_drawn(unsigned int new_n_label_tiles_drawn)
{
    if (m_n_label_tiles_drawn == new_n_label_tiles_drawn)
        return;
    m_n_label_tiles_drawn = new_n_label_tiles_drawn;
    emit n_label_tiles_drawn_changed(m_n_label_tiles_drawn);
}

unsigned int TileStatistics::n_geometry_tiles_gpu() const { return m_n_geometry_tiles_gpu; }

void TileStatistics::set_n_geometry_tiles_gpu(unsigned int new_n_geometry_tiles_gpu)
{
    if (m_n_geometry_tiles_gpu == new_n_geometry_tiles_gpu)
        return;
    m_n_geometry_tiles_gpu = new_n_geometry_tiles_gpu;
    emit n_geometry_tiles_gpu_changed(m_n_geometry_tiles_gpu);
}

unsigned int TileStatistics::n_geometry_tiles_drawn() const { return m_n_geometry_tiles_drawn; }

void TileStatistics::set_n_geometry_tiles_drawn(unsigned int new_n_geometry_tiles_drawn)
{
    if (m_n_geometry_tiles_drawn == new_n_geometry_tiles_drawn)
        return;
    m_n_geometry_tiles_drawn = new_n_geometry_tiles_drawn;
    emit n_geometry_tiles_drawn_changed(m_n_geometry_tiles_drawn);
}

unsigned int TileStatistics::n_ortho_tiles_gpu() const { return m_n_ortho_tiles_gpu; }

void TileStatistics::set_n_ortho_tiles_gpu(unsigned int new_n_ortho_tiles_gpu)
{
    if (m_n_ortho_tiles_gpu == new_n_ortho_tiles_gpu)
        return;
    m_n_ortho_tiles_gpu = new_n_ortho_tiles_gpu;
    emit n_ortho_tiles_gpu_changed(m_n_ortho_tiles_gpu);
}

void TileStatistics::update_gpu_tile_stats(std::unordered_map<std::string, unsigned int> stats)
{
    set_n_label_tiles_gpu(stats["n_label_tiles"]);
    set_n_label_tiles_drawn(stats["n_label_tiles_drawn"]);
    set_n_geometry_tiles_gpu(stats["n_geometry_tiles"]);
    set_n_geometry_tiles_drawn(stats["n_geometry_tiles_drawn"]);
    set_n_ortho_tiles_gpu(stats["n_ortho_tiles"]);
}

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

#pragma once

#include <QObject>

class TileStatistics : public QObject {
    Q_OBJECT
public:
    explicit TileStatistics(QObject* parent = nullptr);
    Q_PROPERTY(unsigned int n_label_tiles_gpu READ n_label_tiles_gpu NOTIFY n_label_tiles_gpu_changed FINAL)
    Q_PROPERTY(unsigned int n_label_tiles_drawn READ n_label_tiles_drawn NOTIFY n_label_tiles_drawn_changed FINAL)
    Q_PROPERTY(unsigned int n_geometry_tiles_gpu READ n_geometry_tiles_gpu NOTIFY n_geometry_tiles_gpu_changed FINAL)
    Q_PROPERTY(unsigned int n_geometry_tiles_drawn READ n_geometry_tiles_drawn NOTIFY n_geometry_tiles_drawn_changed FINAL)
    Q_PROPERTY(unsigned int n_ortho_tiles_gpu READ n_ortho_tiles_gpu NOTIFY n_ortho_tiles_gpu_changed FINAL)

    [[nodiscard]] unsigned int n_label_tiles_gpu() const;
    void set_n_label_tiles_gpu(unsigned int new_n_label_tiles_gpu);

    [[nodiscard]] unsigned int n_label_tiles_drawn() const;
    void set_n_label_tiles_drawn(unsigned int new_n_label_tiles_drawn);

    [[nodiscard]] unsigned int n_geometry_tiles_gpu() const;
    void set_n_geometry_tiles_gpu(unsigned int new_n_geometry_tiles_gpu);

    [[nodiscard]] unsigned int n_geometry_tiles_drawn() const;
    void set_n_geometry_tiles_drawn(unsigned int new_n_geometry_tiles_drawn);

    [[nodiscard]] unsigned int n_ortho_tiles_gpu() const;
    void set_n_ortho_tiles_gpu(unsigned int new_n_ortho_tiles_gpu);

public slots:
    void update_gpu_tile_stats(std::unordered_map<std::string, unsigned> stats);

signals:
    void n_label_tiles_gpu_changed(unsigned int n_label_tiles_gpu);
    void n_label_tiles_drawn_changed(unsigned int n_label_tiles_drawn);
    void n_geometry_tiles_gpu_changed(unsigned int n_geometry_tiles_gpu);
    void n_geometry_tiles_drawn_changed(unsigned int n_geometry_tiles_drawn);
    void n_ortho_tiles_gpu_changed(unsigned int n_ortho_tiles_gpu);

private:
    unsigned m_n_label_tiles_gpu = 0;
    unsigned m_n_label_tiles_drawn = 0;
    unsigned m_n_geometry_tiles_gpu = 0;
    unsigned m_n_geometry_tiles_drawn = 0;
    unsigned m_n_ortho_tiles_gpu = 0;
};

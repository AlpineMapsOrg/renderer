/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
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
#include <nucleus/tile_scheduler/Scheduler.h>
#include <nucleus/vector_tile/types.h>

namespace nucleus::map_label {

class Scheduler : public nucleus::tile_scheduler::Scheduler {
    Q_OBJECT
public:
    explicit Scheduler(QObject* parent = nullptr);
    ~Scheduler() override;

    void set_geometry_ram_cache(nucleus::tile_scheduler::Cache<nucleus::tile_scheduler::tile_types::LayeredTileQuad>* new_geometry_ram_cache);

signals:
    void gpu_tiles_updated(const std::vector<vector_tile::PoiTile>& new_quads, const std::vector<tile::Id>& deleted_quads);

protected:
    void transform_and_emit(const std::vector<tile_scheduler::tile_types::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads) override;
    bool is_ready_to_ship(const nucleus::tile_scheduler::tile_types::DataQuad& quad) const override;

private:
    nucleus::tile_scheduler::Cache<nucleus::tile_scheduler::tile_types::LayeredTileQuad>* m_geometry_ram_cache = nullptr;
};

} // namespace nucleus::map_label

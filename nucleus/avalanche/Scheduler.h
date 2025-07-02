/*****************************************************************************
 * AlpineMaps.org
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

#include "eaws.h"
#include <nucleus/tile/Scheduler.h>
#include <nucleus/tile/types.h>

namespace nucleus::avalanche {

class Scheduler : public nucleus::tile::Scheduler {
    Q_OBJECT
public:
    Scheduler(const Scheduler::Settings& settings, std::shared_ptr<UIntIdManager> m_internal_id_manager);
    ~Scheduler();
    static nucleus::Raster<glm::uint16> to_raster(
        const nucleus::tile::DataQuad& quad, const nucleus::Raster<glm::uint16>& default_raster, std::shared_ptr<UIntIdManager> uint_id_manager);

signals:
    void gpu_tiles_updated(const std::vector<nucleus::tile::Id>& deleted_quads, const std::vector<nucleus::tile::GpuEawsTile>& new_tiles);

protected:
    void transform_and_emit(const std::vector<nucleus::tile::DataQuad>& new_quads, const std::vector<nucleus::tile::Id>& deleted_quads) override;

private:
    nucleus::Raster<glm::uint16> m_default_raster;
    std::shared_ptr<UIntIdManager> m_uint_id_manager;
};

} // namespace nucleus::avalanche

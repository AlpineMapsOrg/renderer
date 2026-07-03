/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2026 Wendelin Muth
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

#include "Scheduler.h"
#include "nucleus/Raster3D.h"
#include "types.h"

namespace nucleus::tile {

class Texture3DScheduler : public Scheduler {
    Q_OBJECT
public:
    Texture3DScheduler(const Scheduler::Settings& settings);
    ~Texture3DScheduler() override;

signals:
    void gpu_tiles_updated(const std::vector<tile::Id>& deleted_tiles, const std::vector<GpuTexture3DTile>& new_tiles);

protected:
    void transform_and_emit(const std::vector<tile::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads) override;
};

} // namespace nucleus::tile

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

#include "GeometryScheduler.h"

#include "utils.h"
#include <QDebug>
#include <nucleus/tile/conversion.h>
#include <nucleus/utils/error.h>
#include <nucleus/utils/image_loader.h>

namespace nucleus::tile {

GeometryScheduler::GeometryScheduler()
    : nucleus::tile::Scheduler(256)
    , m_default_raster(glm::uvec2(65), uint16_t(0))
{
}

GeometryScheduler::~GeometryScheduler() = default;

void GeometryScheduler::transform_and_emit(const std::vector<tile::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    std::vector<GpuGeometryQuad> new_gpu_quads;
    new_gpu_quads.reserve(new_quads.size());

    std::transform(new_quads.cbegin(), new_quads.cend(), std::back_inserter(new_gpu_quads), [this](const auto& quad) {
        using namespace nucleus::utils;

        // create GpuQuad based on cpu quad
        GpuGeometryQuad gpu_quad;
        gpu_quad.id = quad.id;
        assert(quad.n_tiles == 4);
        for (unsigned i = 0; i < 4; ++i) {
            gpu_quad.tiles[i].id = quad.tiles[i].id;
            gpu_quad.tiles[i].bounds = aabb_decorator()->aabb(quad.tiles[i].id);

            if (quad.tiles[i].data->size()) {
                // Height image is available
                auto height_raster = image_loader::rgba8(*quad.tiles[i].data).and_then(error::wrap_to_expected(conversion::to_u16raster)).value_or(m_default_raster);
                gpu_quad.tiles[i].surface = std::make_shared<nucleus::Raster<uint16_t>>(std::move(height_raster));
            } else {
                // Height image is not available (use black default tile)
                gpu_quad.tiles[i].surface = std::make_shared<nucleus::Raster<uint16_t>>(m_default_raster);
            }
        }
        return gpu_quad;
    });

    emit gpu_quads_updated(new_gpu_quads, deleted_quads);
}

void GeometryScheduler::set_texture_compression_algorithm(nucleus::utils::ColourTexture::Format compression_algorithm) { m_compression_algorithm = compression_algorithm; }

} // namespace nucleus::tile

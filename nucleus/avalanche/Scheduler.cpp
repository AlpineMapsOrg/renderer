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

#include "Scheduler.h"
#include "eaws.h"
#include <nucleus/utils/image_loader.h>

namespace nucleus::avalanche {

Scheduler::Scheduler(const Settings& settings, std::shared_ptr<UIntIdManager> internal_id_manager)
    : nucleus::tile::Scheduler(settings)
    , m_default_raster(glm::uvec2(settings.tile_resolution), 0)
    , m_uint_id_manager(internal_id_manager)
{
    m_max_tile_zoom_level = 10;
    UIntIdManager* ptr = m_uint_id_manager.get();
    std::cout << ptr;
}

Scheduler::~Scheduler() = default;

void Scheduler::transform_and_emit(const std::vector<nucleus::tile::DataQuad>& new_quads, const std::vector<nucleus::tile::Id>& deleted_quads)
{
    std::vector<nucleus::tile::GpuEawsQuad> new_gpu_quads;
    new_gpu_quads.reserve(new_quads.size());

    // todo: merge quads, look at texture scheduler
    Q_UNUSED(deleted_quads)

    std::transform(new_quads.cbegin(), new_quads.cend(), std::back_inserter(new_gpu_quads), [this](const auto& quad) {
        // create GpuQuad based on cpu quad
        nucleus::tile::GpuEawsQuad gpu_quad;
        gpu_quad.id = quad.id;
        assert(quad.n_tiles == 4);
        for (unsigned i = 0; i < 4; ++i) {
            gpu_quad.tiles[i].id = quad.tiles[i].id;
            glm::ivec3 CURRENT_ID(quad.tiles[i].id.zoom_level, quad.tiles[i].id.coords);

            if (quad.tiles[i].data->size()) {
                tl::expected<RegionTile, QString> result = vector_tile_reader(*quad.tiles[i].data, quad.tiles[i].id);
                if (result.has_value()) {
                    // create qimage with color coded eaws regions for current tile
                    RegionTile eaws_region_tile = result.value();
                    QImage eawsImage = draw_regions(eaws_region_tile, m_uint_id_manager, 256, 256, quad.tiles[i].id);

                    // Convert Qimage to raster with a 16bit uint region id
                    nucleus::Raster<glm::uint16> eaws_raster_16bit(glm::uvec2(256, 256), 0);
                    for (int i = 0; i < 256; i++) {
                        for (int j = 0; j < 256; j++) {
                            glm::u8vec4 color_vector_8bit(0, 0, 0, 0);
                            QColor color = eawsImage.pixelColor(QPoint(i, j));
                            color_vector_8bit.x = static_cast<uint8_t>(color.red());
                            color_vector_8bit.y = static_cast<uint8_t>(color.green());
                            eaws_raster_16bit.pixel(glm::uvec2(i, j)) = 256 * color_vector_8bit.x + color_vector_8bit.y;
                        }
                    }

                    // Create texture from raster
                    gpu_quad.tiles[i].texture = std::make_shared<nucleus::Raster<glm::uint16>>(eaws_raster_16bit);
                } else {
                    // Eaws image is not available (use 0)
                    gpu_quad.tiles[i].texture = std::make_shared<nucleus::Raster<glm::uint16>>(m_default_raster);
                }
            } else {
                // Ortho image is not available (use 0)
                gpu_quad.tiles[i].texture = std::make_shared<nucleus::Raster<glm::uint16>>(m_default_raster);
            }
        }
        return gpu_quad;
    });

    // emit gpu_quads_updated(new_gpu_quads, deleted_quads);
}

} // namespace nucleus::avalanche

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
    std::vector<nucleus::tile::GpuEawsTile> new_gpu_tiles;
    new_gpu_tiles.reserve(new_quads.size());

    Q_UNUSED(deleted_quads)
    for (const auto& quad : new_quads) {
        nucleus::tile::GpuEawsTile gpu_tile_from_quad;
        gpu_tile_from_quad.id = quad.id;
        nucleus::Raster<glm::uint16> quad_as_raster = to_raster(quad, m_default_raster, m_uint_id_manager);
        gpu_tile_from_quad.texture = std::make_shared<nucleus::Raster<glm::uint16>>(quad_as_raster);
        new_gpu_tiles.push_back(gpu_tile_from_quad);
    }

    emit gpu_tiles_updated(new_gpu_tiles, deleted_quads);
}

nucleus::Raster<glm::uint16> to_raster(
    const nucleus::tile::DataQuad& quad, const nucleus::Raster<glm::uint16>& default_raster, std::shared_ptr<UIntIdManager> uint_id_manager)
{
    std::array<Raster<glm::uint16>, 4> quad_rasters;
    std::array<tile::Id, 4> quad_ids;
    for (const auto& tile : quad.tiles) {
        const auto quad_index = unsigned(quad_position(tile.id));
        quad_ids[quad_index] = tile.id;
        if (!tile.data->size()) {
            // Data not available use default raster
            quad_rasters[quad_index] = default_raster;
            continue;
        }

        // Read vector tile from data
        tl::expected<RegionTile, QString> result = vector_tile_reader(*tile.data, tile.id);

        // could not read vector tile from data, use default raster
        if (!result.has_value()) {
            quad_rasters[quad_index] = default_raster;
            continue;
        }

        // Reading tile worked. Create qimage with color coded eaws regions for current tile
        RegionTile eaws_region_tile = result.value();
        QImage eawsImage = draw_regions(eaws_region_tile, uint_id_manager, 256, 256, tile.id);

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

        // Collect raster of current tile in quad
        quad_rasters[quad_index] = nucleus::Raster<glm::uint16>(eaws_raster_16bit);
    }

    // Merge 4 tiles from quad into one raster representing the quad
    nucleus::Raster<glm::uint16> quad_as_raster
        = nucleus::concatenate_horizontally(quad_rasters[unsigned(tile::QuadPosition::TopLeft)], quad_rasters[unsigned(tile::QuadPosition::TopRight)]);
    quad_as_raster.append_vertically(
        nucleus::concatenate_horizontally(quad_rasters[unsigned(tile::QuadPosition::BottomLeft)], quad_rasters[unsigned(tile::QuadPosition::BottomRight)]));

    // return raster represntation of provided quad
    return quad_as_raster;
}

} // namespace nucleus::avalanche

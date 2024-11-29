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

#include "EawsTextureScheduler.h"
#include "nucleus/avalanche/eaws.h"
#include <nucleus/utils/image_loader.h>

namespace avalanche::eaws {
TextureScheduler::TextureScheduler(std::string name, unsigned texture_resolution, QObject* parent)
    : nucleus::tile::Scheduler(std::move(name), texture_resolution, parent)
    , m_default_raster(glm::uvec2(texture_resolution), { 255, 255, 255, 255 })
{
    m_max_tile_zoom_level = 10;
}

TextureScheduler::~TextureScheduler() = default;

void TextureScheduler::transform_and_emit(const std::vector<nucleus::tile::DataQuad>& new_quads, const std::vector<nucleus::tile::Id>& deleted_quads)
{
    std::vector<nucleus::tile::GpuTextureQuad> new_gpu_quads;
    new_gpu_quads.reserve(new_quads.size());

    std::transform(new_quads.cbegin(), new_quads.cend(), std::back_inserter(new_gpu_quads), [this](const auto& quad) {
        // create GpuQuad based on cpu quad
        nucleus::tile::GpuTextureQuad gpu_quad;
        gpu_quad.id = quad.id;
        assert(quad.n_tiles == 4);
        for (unsigned i = 0; i < 4; ++i) {
            gpu_quad.tiles[i].id = quad.tiles[i].id;
            glm::ivec3 CURRENT_ID(quad.tiles[i].id.zoom_level, quad.tiles[i].id.coords);

            if (quad.tiles[i].data->size()) {
                tl::expected<avalanche::eaws::RegionTile, QString> result = avalanche::eaws::vector_tile_reader(*quad.tiles[i].data, quad.tiles[i].id);
                if (result.has_value()) {
                    // create qimage with color coded eaws regions for current tile
                    avalanche::eaws::RegionTile eaws_region_tile = result.value();
                    QImage eawsImage = avalanche::eaws::draw_regions(eaws_region_tile, &m_internal_id_manager, 256, 256, quad.tiles[i].id);

                    // Convert Qimage to raster with 8bit vectors (pixel by pixel)
                    nucleus::Raster<glm::u8vec4> eaws_color_raster_8bit(glm::uvec2(256, 256), { 0, 0, 0, 255 });
                    for (int i = 0; i < 256; i++) {
                        for (int j = 0; j < 256; j++) {
                            glm::u8vec4 color_vector_8bit(0, 0, 0, 255);
                            QColor color = eawsImage.pixelColor(QPoint(i, j));
                            color_vector_8bit.x = static_cast<uint8_t>(color.red());
                            color_vector_8bit.y = static_cast<uint8_t>(color.green());
                            color_vector_8bit.z = 0;
                            eaws_color_raster_8bit.pixel(glm::uvec2(i, j)) = color_vector_8bit;
                        }
                    }

                    // Create texture from raster
                    gpu_quad.tiles[i].texture = std::make_shared<nucleus::utils::MipmappedColourTexture>(generate_mipmapped_colour_texture(eaws_color_raster_8bit, m_compression_algorithm));
                } else {
                    // Eaws image is not available (use white default tile)
                    gpu_quad.tiles[i].texture = std::make_shared<nucleus::utils::MipmappedColourTexture>(generate_mipmapped_colour_texture(m_default_raster, m_compression_algorithm));
                }
            } else {
                // Ortho image is not available (use white default tile)
                gpu_quad.tiles[i].texture = std::make_shared<nucleus::utils::MipmappedColourTexture>(generate_mipmapped_colour_texture(m_default_raster, m_compression_algorithm));
            }
        }
        return gpu_quad;
    });

    emit gpu_quads_updated(new_gpu_quads, deleted_quads);
}

} // namespace avalanche::eaws

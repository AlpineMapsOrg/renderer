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

#include "TextureScheduler.h"
#include "conversion.h"
#include <QDebug>
#include <nucleus/utils/image_loader.h>

namespace nucleus::tile {

TextureScheduler::TextureScheduler(unsigned texture_resolution, QObject* parent)
    : nucleus::tile::Scheduler(texture_resolution, parent)
    , m_default_raster(glm::uvec2(texture_resolution), { 255, 255, 255, 255 })
{
}

TextureScheduler::~TextureScheduler() = default;

void TextureScheduler::transform_and_emit(const std::vector<tile::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    // std::vector<GpuTextureQuad> new_gpu_quads;
    // new_gpu_quads.reserve(new_quads.size());

    // std::transform(new_quads.cbegin(), new_quads.cend(), std::back_inserter(new_gpu_quads), [this](const auto& quad) {
    //     // create GpuQuad based on cpu quad
    //     GpuTextureQuad gpu_quad;
    //     gpu_quad.id = quad.id;
    //     assert(quad.n_tiles == 4);
    //     for (unsigned i = 0; i < 4; ++i) {
    //         gpu_quad.tiles[i].id = quad.tiles[i].id;

    //         if (quad.tiles[i].data->size()) {
    //             // Ortho image is available
    //             const auto ortho_raster = nucleus::utils::image_loader::rgba8(*quad.tiles[i].data.get()).value_or(m_default_raster);
    //             gpu_quad.tiles[i].texture = std::make_shared<nucleus::utils::MipmappedColourTexture>(generate_mipmapped_colour_texture(ortho_raster, m_compression_algorithm));
    //         } else {
    //             // Ortho image is not available (use white default tile)
    //             gpu_quad.tiles[i].texture = std::make_shared<nucleus::utils::MipmappedColourTexture>(generate_mipmapped_colour_texture(m_default_raster, m_compression_algorithm));
    //         }
    //     }
    //     return gpu_quad;
    // });

    std::vector<GpuTextureTile> new_gpu_tiles;
    new_gpu_tiles.reserve(new_gpu_tiles.size() * 4);

    for (const auto& quad : new_quads) {
        GpuTextureTile gpu_tile;
        gpu_tile.id = quad.id;
        auto ortho_raster = to_raster(quad, m_default_raster);
        gpu_tile.texture = std::make_shared<nucleus::utils::MipmappedColourTexture>(generate_mipmapped_colour_texture(ortho_raster, m_compression_algorithm));
        new_gpu_tiles.push_back(gpu_tile);
    }

    emit gpu_tiles_updated(deleted_quads, new_gpu_tiles);
}

void TextureScheduler::set_texture_compression_algorithm(nucleus::utils::ColourTexture::Format compression_algorithm) { m_compression_algorithm = compression_algorithm; }

Raster<glm::u8vec4> TextureScheduler::to_raster(const tile::DataQuad& quad, const Raster<glm::u8vec4>& default_raster)
{
    assert(quad.n_tiles == 4);

    std::array<Raster<glm::u8vec4>, 4> quad_rasters;
    std::array<tile::Id, 4> quad_ids;
    for (const auto& tile : quad.tiles) {
        const auto quad_index = unsigned(quad_position(tile.id));
        quad_ids[quad_index] = tile.id;
        if (tile.data->size()) {
            // Ortho image is available
            const auto ortho_raster = nucleus::utils::image_loader::rgba8(*tile.data.get()).value_or(default_raster);
            quad_rasters[quad_index] = std::move(ortho_raster);
        } else {
            // Ortho image is not available (use white default tile)
            quad_rasters[quad_index] = default_raster;
        }
    }

    auto ortho_raster = nucleus::concatenate_horizontally(quad_rasters[unsigned(tile::QuadPosition::TopLeft)], quad_rasters[unsigned(tile::QuadPosition::TopRight)]);
    ortho_raster.append_vertically(nucleus::concatenate_horizontally(quad_rasters[unsigned(tile::QuadPosition::BottomLeft)], quad_rasters[unsigned(tile::QuadPosition::BottomRight)]));

    return ortho_raster;
}

} // namespace nucleus::tile

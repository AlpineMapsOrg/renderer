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

#include "Texture3DScheduler.h"
#include "conversion.h"
#include <QDebug>
#include <extern/libktx/lib/include/ktx.h>


namespace nucleus::tile {

Texture3DScheduler::Texture3DScheduler(const Scheduler::Settings& settings)
    : nucleus::tile::Scheduler(settings)
{
}

Texture3DScheduler::~Texture3DScheduler() = default;

void Texture3DScheduler::transform_and_emit(const std::vector<tile::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads)
{
    // Stitching 2x2 3d tiles is possible, but it doesn't really make sense (to me).
    std::vector<GpuTexture3DTile> new_gpu_tiles;
    new_gpu_tiles.reserve(new_quads.size() * 4);

    for (const auto& quad : new_quads) {
        for (size_t i = 0; i < 4; i++) {
            const auto& tile = quad.tiles[i];

            if (tile.data->size() == 0)
                continue;

            // NOTE: This implementation is quite specific to the cloud texture loading. Not intended for general purpose use.

            GpuTexture3DTile gpu_tile;
            gpu_tile.id = tile.id;
            auto&& texture = std::make_shared<nucleus::utils::MipmappedColourTexture3D>();
            gpu_tile.texture = texture;

            ktxTexture* ktx = nullptr;
            ktxTexture_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(tile.data.get()->data()), tile.data->size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx);

            texture->reserve(ktx->numLevels);
            ktxTexture_IterateLevelFaces(ktx, [](int /*miplevel*/, int /*face*/, int width, int height, int depth, ktx_uint64_t faceLodSize, void *pixels, void *userdata) {
                auto* result = static_cast<std::vector<nucleus::utils::ColourTexture3D>*>(userdata);
                std::span byte_span{static_cast<uint8_t*>(pixels), static_cast<size_t>(faceLodSize)};
                result->emplace_back(byte_span, width, height, depth, nucleus::utils::ColourTexture3D::Format::BC4_UNORM);
                return KTX_SUCCESS;
            }, texture.get());
            
            new_gpu_tiles.push_back(gpu_tile);
        }
    }

    std::vector<tile::Id> deleted_tiles;
    deleted_tiles.reserve(deleted_quads.size() * 4);
    for (const auto & deleted_quad : deleted_quads) {
        auto&& children = deleted_quad.children();
        deleted_tiles.push_back(children[0]);
        deleted_tiles.push_back(children[1]);
        deleted_tiles.push_back(children[2]);
        deleted_tiles.push_back(children[3]);
    }

    // we are merging the tiles. so deleted quads become deleted tiles.
    emit gpu_tiles_updated(deleted_tiles, new_gpu_tiles);
}


} // namespace nucleus::tile

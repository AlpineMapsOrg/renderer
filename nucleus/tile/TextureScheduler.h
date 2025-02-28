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

#include "Scheduler.h"
#include "types.h"

namespace nucleus::tile {

class TextureScheduler : public Scheduler {
    Q_OBJECT
public:
    TextureScheduler(unsigned texture_resolution, QObject* parent = nullptr);
    ~TextureScheduler() override;

    void set_texture_compression_algorithm(nucleus::utils::ColourTexture::Format compression_algorithm);

signals:
    void gpu_quads_updated(const std::vector<GpuTextureQuad>& new_quads, const std::vector<tile::Id>& deleted_quads);

protected:
    void transform_and_emit(const std::vector<tile::DataQuad>& new_quads, const std::vector<tile::Id>& deleted_quads) override;

private:
    nucleus::utils::ColourTexture::Format m_compression_algorithm = nucleus::utils::ColourTexture::Format::Uncompressed_RGBA;
    Raster<glm::u8vec4> m_default_raster;
};

} // namespace nucleus::tile

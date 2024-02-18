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

#include <QImage>
#include <nucleus/Raster.h>

namespace nucleus::utils {

class CompressedTexture {
public:
    enum class Algorithm { Uncompressed_RGBA, DXT1, ETC1 };

private:
    std::vector<uint8_t> m_data;
    unsigned m_width = 0;
    unsigned m_height = 0;
    Algorithm m_algorithm = Algorithm::Uncompressed_RGBA;

public:
    explicit CompressedTexture(const QImage& image, Algorithm algorithm);
    const uint8_t* data() const { return m_data.data(); }
    size_t n_bytes() const { return m_data.size(); }
    unsigned width() const { return m_width; }
    unsigned height() const { return m_height; }
};

} // namespace nucleus::utils

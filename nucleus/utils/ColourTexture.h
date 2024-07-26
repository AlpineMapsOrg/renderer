/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include <vector>
#include <glm/glm.hpp>
#include "nucleus/Raster.h"

namespace nucleus::utils {

class ColourTexture {
public:
    enum class Format { Uncompressed_RGBA, DXT1, ETC1 };

private:
    std::vector<uint8_t> m_data;
    unsigned m_width = 0;
    unsigned m_height = 0;
    Format m_format = Format::Uncompressed_RGBA;

public:
    explicit ColourTexture(const nucleus::Raster<glm::u8vec4>& data, Format format);
    [[nodiscard]] const uint8_t* data() const { return m_data.data(); }
    [[nodiscard]] size_t n_bytes() const { return m_data.size(); }
    [[nodiscard]] unsigned width() const { return m_width; }
    [[nodiscard]] unsigned height() const { return m_height; }
    [[nodiscard]] Format format() const { return m_format; }
};

} // namespace nucleus::utils

/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "nucleus/Raster3D.h"

#include <glm/glm.hpp>
#include <span>
#include <vector>

namespace nucleus::utils {

class ColourTexture3D {
public:
    enum class Format { R8_UNORM, BC4_UNORM };

private:
    std::vector<uint8_t> m_data;
    unsigned m_width = 0;
    unsigned m_height = 0;
    unsigned m_depth = 0;
    Format m_format = Format::R8_UNORM;

public:
    explicit ColourTexture3D(const nucleus::Raster3D<glm::u8vec4>& data, Format format);
    ColourTexture3D(std::span<uint8_t> data, unsigned width, unsigned height, unsigned depth, Format format);
    [[nodiscard]] const uint8_t* data() const { return m_data.data(); }
    [[nodiscard]] size_t n_bytes() const { return m_data.size(); }
    [[nodiscard]] unsigned width() const { return m_width; }
    [[nodiscard]] unsigned height() const { return m_height; }
    [[nodiscard]] unsigned depth() const { return m_depth; }
    [[nodiscard]] Format format() const { return m_format; }
};

using MipmappedColourTexture3D = std::vector<ColourTexture3D>;

} // namespace nucleus::utils

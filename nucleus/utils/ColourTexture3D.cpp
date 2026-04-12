/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2026 Wendelin Muth
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

#include "ColourTexture3D.h"

nucleus::utils::ColourTexture3D::ColourTexture3D(const nucleus::Raster3D<glm::u8vec4>& image, Format format)
    : m_data(reinterpret_cast<const uint8_t*>(image.data()), reinterpret_cast<const uint8_t*>(image.data()) + image.size_in_bytes())
    , m_width(unsigned(image.width()))
    , m_height(unsigned(image.height()))
    , m_depth(unsigned(image.depth()))
    , m_format(format)
{
}

nucleus::utils::ColourTexture3D::ColourTexture3D(std::span<uint8_t> data, unsigned width, unsigned height, unsigned depth, Format format)
    : m_data(data.begin(), data.end())
    , m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_format(format)
{
}
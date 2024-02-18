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

namespace nucleus::utils::texture_compression {

enum class Algorithm { Uncompressed_RGBA, DXT1, ETC1 };

std::vector<uint8_t> to_dxt1(const QImage& image);
std::vector<uint8_t> to_etc1(const QImage& image);
std::vector<uint8_t> to_uncompressed_rgba(const QImage& image);

std::vector<uint8_t> to_compressed(const QImage& image, Algorithm algorithm);
}

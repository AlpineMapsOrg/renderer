/*****************************************************************************
 * AlpineMaps.org
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

#include <nucleus/Raster.h>
#include <QByteArray>

namespace nucleus::utils::image_writer {

void rgba8_as_png(const Raster<glm::u8vec4>& data, const QString& filename);

void rgba8_as_png(const QByteArray& data, const glm::uvec2& resolution, const QString& filename);

} // namespace nucleus::utils::image_writer

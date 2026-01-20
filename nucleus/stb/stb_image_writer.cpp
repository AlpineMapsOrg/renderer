/*****************************************************************************
 * Alpine Maps and weBIGeo
 * Copyright (C) 2024 Patrick Komon
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

#include "stb_image_writer.h"
#include <stb_slim/stb_image_write.h>

#include <QFile>

namespace nucleus::stb {

void write_8bit_rgba_image_to_file_bmp(const QByteArray& data, unsigned int width, unsigned int height, const QString& filename)
{
    assert(width > 0);
    assert(height > 0);

    stbi_write_bmp(filename.toUtf8().constData(), width, height, 4, data.data());
}

} // namespace nucleus::stb

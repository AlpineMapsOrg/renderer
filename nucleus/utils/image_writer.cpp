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

#include "image_writer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION // Enable implementation
#include <stb_slim/stb_image_write.h>

#include <QDebug>
#include <QFile>

namespace nucleus::utils::image_writer {

void rgba8_as_png(const Raster<glm::u8vec4>& data, const QString& filename)
{
    assert(data.width() > 0);
    assert(data.height() > 0);

    int result = stbi_write_png(filename.toUtf8().constData(), // File name
        data.width(), // Image width
        data.height(), // Image height
        4, // Number of color components (e.g., 2 for grayscale with alpha)
        data.data(), // Pointer to the image data
        data.width() * 4 // Stride in bytes (width * number of components)
    );

    if (result == 0) {
        qCritical() << "Failed to write image" << filename;
    }
}

void rgba8_as_png(const QByteArray& data, const glm::uvec2& resolution, const QString& filename)
{
    assert(resolution.x > 0);
    assert(resolution.y > 0);
    assert(data.size() == static_cast<int>(resolution.x * resolution.y * 4)); // Ensure data size matches resolution

    int result = stbi_write_png(filename.toUtf8().constData(), // File name
        static_cast<int>(resolution.x), // Image width
        static_cast<int>(resolution.y), // Image height
        4, // Number of color components (RGBA)
        data.constData(), // Pointer to the image data
        static_cast<int>(resolution.x * 4) // Stride in bytes (width * number of components)
    );

    if (result == 0) {
        qCritical() << "Failed to write image" << filename;
    }
}

} // namespace nucleus::utils::image_writer

/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include "image_loader.h"

// Limit the dimensions of images to 8192x8192. This is already quite restricting
// in terms of that a lot of GPUs don't support textures that large. Make sure
// you know what you are doing, before you change this value.
#define STBI_MAX_DIMENSIONS 8192

// Only include the code for the formats we need. This reduces the code footprint
// and allows for faster compilation times. Add more formats here, if you need them.
// For possible options check the stb_image.h file.
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG

// Remove if you intend to use stbi_failure_reason()
#define STBI_NO_FAILURE_STRINGS

#define STB_IMAGE_IMPLEMENTATION
#include <stb_slim/stb_image.h>

#include <stdexcept>
#include <QFile>

namespace nucleus::utils::image_loader {

Raster<glm::u8vec4> rgba8(const QByteArray& byteArray)
{
    int width, height, channels;
    const int requested_channels = 4; // Request 4 channels to always get RGBA8 images
    const stbi_uc* source_data = reinterpret_cast<const stbi_uc*>(byteArray.constData());
    unsigned char* data = stbi_load_from_memory(
        source_data,
        byteArray.size(),
        &width, &height, &channels,
        requested_channels
        );

    if (data == nullptr) {
        throw std::runtime_error("Failed to load image from bytearray");
    }

    // NOTE: We copy the contents of the data pointer into a Raster object. Sadly
    // we can't use the allocated memory directly, because for that we would need a custom
    // allocator for the std::vector class.
    Raster<glm::u8vec4> raster(glm::uvec2(width, height));
    memcpy(raster.data(), data, raster.size_in_bytes());
    stbi_image_free(data);

    return raster;
}

Raster<glm::u8vec4> rgba8(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open file: " + filename.toStdString());
    }

    QByteArray byteArray = file.readAll();
    file.close();

    // NOTE: We don't use the stb_image loader directly, because QFile can load from ressources
    return rgba8(byteArray);
}

Raster<glm::u8vec4> rgba8(const char* filename) { return rgba8(QString(filename)); }

} // namespace nucleus::utils::image_loader

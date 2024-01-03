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

#define GOOFYTC_IMPLEMENTATION
#include <GoofyTC/goofy_tc.h>

#include "texture_compression.h"

std::vector<u_char> nucleus::utils::texture_compression::to_dxt1(const QImage& qimage)
{
    std::vector<u_char> compressed;
    compressed.resize(256 * 256 * 2);
    if (qimage.format() != QImage::Format_ARGB32 && qimage.format() != QImage::Format_RGB32) {
        // let's hope that the format is always ARGB32
        // if not, please implement the conversion, that'll give better performance.
        // the assert will be disabled in release, just as a backup.
        assert(false);
        return to_dxt1(qimage.convertedTo(QImage::Format_ARGB32));
    }
    const auto result = goofy::compressDXT1(compressed.data(), qimage.bits(), qimage.width(), qimage.height(), qimage.width());
    assert(result == 0);

    return compressed;
}

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

std::vector<uint8_t> nucleus::utils::texture_compression::to_dxt1(const QImage& qimage)
{
    if (qimage.format() != QImage::Format_RGBA8888 && qimage.format() != QImage::Format_RGBX8888) {
        // format is ARGB32 most of the time. we could implement on-the-fly transformation in goofy::compressDXT1 if this is too slow.
        return to_dxt1(qimage.convertedTo(QImage::Format_RGBX8888));
    }
    assert(qimage.width() == qimage.height());
    assert(qimage.width() % 16 == 0);
    assert(qimage.bytesPerLine() * qimage.height() == qimage.width() * qimage.height() * 4);

    struct alignas(16) AlignedBlock
    {
        std::array<uint8_t, 16> data;
    };
    static_assert(sizeof(AlignedBlock) == 16);

    const auto n_bytes_in = qimage.width() * qimage.height() * 4;
    const auto n_bytes_out = qimage.width() * qimage.height() / 2;
    assert(n_bytes_in % sizeof(AlignedBlock) == 0);

    auto aligned_in = std::vector<AlignedBlock>(n_bytes_in / sizeof(AlignedBlock));
    auto data_ptr = reinterpret_cast<uchar*>(aligned_in.data());
    std::copy(qimage.bits(), qimage.bits() + n_bytes_in, data_ptr);

    std::vector<uint8_t> compressed(n_bytes_out);
    const auto result = goofy::compressDXT1(compressed.data(), data_ptr, qimage.width(), qimage.height(), qimage.width() * 4);
    assert(result == 0);

    // std::copy(data_ptr, data_ptr + n_bytes, compressed.begin());

    return compressed;
}

std::vector<uint8_t> nucleus::utils::texture_compression::to_etc1(const QImage& qimage)
{
    if (qimage.format() != QImage::Format_RGBA8888 && qimage.format() != QImage::Format_RGBX8888) {
        // format is ARGB32 most of the time. we could implement on-the-fly transformation in goofy::compressDXT1 if this is too slow.
        return to_etc1(qimage.convertedTo(QImage::Format_RGBX8888));
    }
    assert(qimage.width() == qimage.height());
    assert(qimage.width() % 16 == 0);
    assert(qimage.bytesPerLine() * qimage.height() == qimage.width() * qimage.height() * 4);

    struct alignas(16) AlignedBlock
    {
        std::array<uint8_t, 16> data;
    };
    static_assert(sizeof(AlignedBlock) == 16);

    const auto n_bytes_in = qimage.width() * qimage.height() * 4;
    const auto n_bytes_out = qimage.width() * qimage.height() / 2;
    assert(n_bytes_in % sizeof(AlignedBlock) == 0);

    auto aligned_in = std::vector<AlignedBlock>(n_bytes_in / sizeof(AlignedBlock));
    auto data_ptr = reinterpret_cast<uchar*>(aligned_in.data());
    std::copy(qimage.bits(), qimage.bits() + n_bytes_in, data_ptr);

    std::vector<uint8_t> compressed(n_bytes_out);
    const auto result = goofy::compressETC1(compressed.data(), data_ptr, qimage.width(), qimage.height(), qimage.width() * 4);
    assert(result == 0);

    // std::copy(data_ptr, data_ptr + n_bytes, compressed.begin());

    return compressed;
}

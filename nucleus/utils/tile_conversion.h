/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <glm/glm.hpp>

#include <QByteArray>
#include "nucleus/Raster.h"

#ifdef QT_GUI_LIB
#include <QImage>
#endif

namespace nucleus::utils::tile_conversion {

/**
 * @brief Converts an RGBA8 raster to a uint16_t raster by packing the rg channels. ba are ignored.
 */
Raster<uint16_t> to_u16raster(const Raster<glm::u8vec4>& raster);

#ifdef QT_GUI_LIB
inline Raster<uint16_t> to_u16raster(const QImage& qimage)
{
    if (qimage.format() != QImage::Format_ARGB32 && qimage.format() != QImage::Format_RGB32) {
        // let's hope that the format is always ARGB32
        // if not, please implement the conversion, that'll give better performance.
        // the assert will be disabled in release, just as a backup.
        assert(false);
        return to_u16raster(qimage.convertedTo(QImage::Format_ARGB32));
    }
    Raster<uint16_t> raster({ qimage.width(), qimage.height() });

    const auto* image_pointer = reinterpret_cast<const uint32_t*>(qimage.constBits());
    for (uint16_t& r : raster) {
        r = uint16_t((*image_pointer) >> 8);
        ++image_pointer;
    }
    return raster;
}

inline nucleus::Raster<glm::u8vec4> to_rgba8raster(const QImage& image)
{
    if (image.format() != QImage::Format_RGBA8888) {
        // let's hope that the format is always Format_ARGB32
        // if not, please implement the conversion, that'll give better performance.
        // the assert will be disabled in release, just as a backup.
        assert(false);
        return to_rgba8raster(image.convertedTo(QImage::Format_RGBA8888));
    }

    nucleus::Raster<glm::u8vec4> raster({ image.width(), image.height() });

    // const auto* image_pointer = reinterpret_cast<const uint32_t*>(image.constBits());
    // for (glm::u8vec4& v : raster) {
    //     v.x = uint8_t(*image_pointer >> 24);
    //     v.y = uint8_t(*image_pointer >> 16);
    //     v.z = uint8_t(*image_pointer >> 8);
    //     v.w = uint8_t(*image_pointer);
    //     ++image_pointer;
    // }
    std::memcpy(raster.data(), image.bits(), image.sizeInBytes());
    return raster;
}
#endif

inline glm::u8vec4 float2alpineRGBA(float height)
{
    const auto r = std::clamp(int(height / 32.0f), 0, 255);
    const auto g = std::clamp(int(std::fmod(height, 32.0f) * 8), 0, 255);

    return { glm::u8(r), glm::u8(g), 0, 255 };
}

inline float alppineRGBA2float(const glm::u8vec4& rgba)
{
    constexpr auto one_red = 32.0f;
    constexpr auto one_green = 32.000000001f / 256;
    return float(rgba.x) * one_red + float(rgba.y) * one_green;
}

inline uint16_t alppineRedGreen2uint16(uchar red, uchar green)
{
    return uint16_t(red << 8) | uint16_t(green);
}
inline uint16_t alppineRGBA2uint16(const glm::u8vec4& rgba)
{
    return alppineRedGreen2uint16(rgba.x, rgba.y);
}
inline glm::u8vec4 uint162alpineRGBA(uint16_t v)
{
    return { v >> 8, v & 255, 0, 255 };
}
}

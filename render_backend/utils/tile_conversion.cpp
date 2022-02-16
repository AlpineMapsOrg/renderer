/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include "tile_conversion.h"

namespace tile_conversion {

Raster<glm::u8vec4> toRasterRGBA(const QByteArray& byte_array)
{
  const auto qimage = toQImage(byte_array).convertedTo(QImage::Format_RGBA8888);
  Raster<glm::u8vec4> retval({qimage.width(), qimage.height()});
  std::copy(qimage.constBits(), qimage.constBits() + qimage.sizeInBytes(), reinterpret_cast<uchar*>(&(retval.begin()->x)));
  return retval;
}

Raster<uint16_t> qImage2uint16Raster(const QImage&  qimage)
{
  if (qimage.format() != QImage::Format_ARGB32) {
    // let's hope that the format is always ARGB32
    // if not, please implement the conversion, that'll give better performance.
    // the assert will be disabled in release, just as a backup.
    assert(false);
    return qImage2uint16Raster(qimage.convertedTo(QImage::Format_ARGB32));
  }
  Raster<uint16_t> raster({qimage.width(), qimage.height()});

  const auto qimage_copy = qimage.mirrored();

  const auto* image_pointer = reinterpret_cast<const u_int32_t*>(qimage_copy.constBits());
  for (uint16_t& r : raster) {
    r = uint16_t((*image_pointer) >> 8);
    ++image_pointer;
  }

  return raster;
}

}

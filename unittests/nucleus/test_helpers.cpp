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

#include <glm/glm.hpp>

#include <QImage>
#include <QBuffer>

namespace test_helpers {

QByteArray white_jpeg_tile(unsigned int size)
{
    QImage default_tile(QSize { int(size), int(size) }, QImage::Format_ARGB32);
    default_tile.fill(Qt::GlobalColor::white);
    QByteArray arr;
    QBuffer buffer(&arr);
    buffer.open(QIODevice::WriteOnly);
    default_tile.save(&buffer, "JPEG");
    return arr;
}

QByteArray black_png_tile(unsigned size)
{
    QImage default_tile(QSize { int(size), int(size) }, QImage::Format_ARGB32);
    default_tile.fill(Qt::GlobalColor::black);
    QByteArray arr;
    QBuffer buffer(&arr);
    buffer.open(QIODevice::WriteOnly);
    default_tile.save(&buffer, "PNG");
    return arr;
}

}

/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek
 * SPDX-License-Identifier: GPL-3.0-or-later
 *****************************************************************************/

#pragma once

#include <QString>
#include <array>

namespace texture_compression_data {

struct TileGroup {
    int zoom;
    int y;
    int x;
};

constexpr std::array<TileGroup, 16> tile_groups { {
    { 17, 45448, 71496 },
    { 16, 22832, 35144 },
    { 16, 23030, 35578 },
    { 13, 2852, 4476 },
    { 14, 5702, 8808 },
    { 15, 11574, 17670 },
    { 16, 23030, 35078 },
    { 15, 11460, 17622 },
    { 14, 5752, 8656 },
    { 16, 23084, 34746 },
    { 14, 5684, 8926 },
    { 15, 11418, 17692 },
    { 16, 22956, 34570 },
    { 15, 11358, 17904 },
    { 16, 22910, 35770 },
    { 14, 5656, 8938 },
} };

inline QString tile_url(const TileGroup& group, int x_offset, int y_offset)
{
    return QStringLiteral("https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/%1/%2/%3.jpeg")
        .arg(group.zoom)
        .arg(group.y + y_offset)
        .arg(group.x + x_offset);
}

} // namespace texture_compression_data

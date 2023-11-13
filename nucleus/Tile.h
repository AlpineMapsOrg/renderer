#pragma once

#include <QImage>
#include <glm/glm.hpp>

#include "Raster.h"
#include "radix/tile.h"

namespace nucleus {
struct Tile {
    Tile() = default;
    Tile(tile::Id id, tile::SrsAndHeightBounds bounds, nucleus::Raster<uint16_t> height_map, QImage orthotexture)
        : id(id)
        , bounds(bounds)
        , height_map(height_map)
        , orthotexture(orthotexture)
    {
    }
    tile::Id id = {};
    tile::SrsAndHeightBounds bounds = {};
    nucleus::Raster<uint16_t> height_map;
    QImage orthotexture;
};
}

#pragma once

#include <QImage>
#include <glm/glm.hpp>

#include "Raster.h"
#include "sherpa/tile.h"

struct Tile {
    Tile() = default;
    Tile(tile::Id id, tile::SrsAndHeightBounds bounds, Raster<uint16_t> height_map, QImage orthotexture)
        : id(id)
        , bounds(bounds)
        , height_map(height_map)
        , orthotexture(orthotexture)
    {
    }
    tile::Id id = {};
    tile::SrsAndHeightBounds bounds = {};
    Raster<uint16_t> height_map;
    QImage orthotexture;
};

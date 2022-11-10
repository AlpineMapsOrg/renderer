#pragma once

#include <QImage>
#include <glm/glm.hpp>

#include "Raster.h"
#include "alpine_renderer/srs.h"

struct Tile {
    Tile() = default;
    Tile(srs::TileId id, srs::Bounds bounds, Raster<uint16_t> height_map, QImage orthotexture)
        : id(id)
        , bounds(bounds)
        , height_map(height_map)
        , orthotexture(orthotexture)
    {
    }
    srs::TileId id = {};
    srs::Bounds bounds = {};
    Raster<uint16_t> height_map;
    QImage orthotexture;
};

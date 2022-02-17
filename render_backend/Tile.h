 #pragma once

#include <glm/glm.hpp>
#include <QImage>

#include "render_backend/srs.h"
#include "Raster.h"


struct Tile {
  Tile() = default;
  Tile(srs::TileId id, srs::Bounds bounds, Raster<uint16_t> height_map, QImage orthotexture) :
    id(id), bounds(bounds), height_map(height_map), orthotexture(orthotexture) {}
  srs::TileId id = {};
  srs::Bounds bounds = {};
  Raster<uint16_t> height_map;
  QImage orthotexture;
};

 #pragma once

#include <glm/glm.hpp>

#include "render_backend/srs.h"
#include "Raster.h"

struct Tile {
  srs::TileId id = {};
  srs::Bounds bounds = {};
  Raster<uint16_t> height_map;
  Raster<glm::u8vec4> orthotexture;
};

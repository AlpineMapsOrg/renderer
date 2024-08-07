/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include <unordered_map>

#include <glm/glm.hpp>

#include <nucleus/Raster.h>
#include <nucleus/vector_tiles/VectorTileFeature.h>

struct stbtt_fontinfo;

namespace nucleus::maplabel {

struct CharData {
    uint16_t x, y, width, height; // coordinates of bbox in bitmap
    float xoff, yoff; // position offsets for e.g. lower/uppercase
};

struct VertexData {
    glm::vec4 position; // start_x, start_y, offset_x, offset_y
    glm::vec4 uv; // start_u, start_v, offset_u, offset_v
    glm::vec3 world_position;
    float importance;
};

struct LabelMeta {
    Raster<glm::u8vec2> font_atlas;
    std::unordered_map<nucleus::vectortile::FeatureType, Raster<glm::u8vec4>> icons;
};

} // namespace nucleus::maplabel

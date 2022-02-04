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

#pragma once

#include <vector>
#include <cassert>


// functions in this file generate the indices for our terrain meshes.
// we use a regular grid. vertex positions are computed in the vertex shader on the fly,
// but we still need the indices of the triangles.
// the triangles are drawn as a triangle strip, and we want as few repetitions as possible.

// tile meshes consist of the visible surface and additional skirts, which help against holes.

// height data stored in raster formats (row, column, first row on top / north) is used to
// displace the vertices vertically. since we don't want to mirror that raster, the vertices
// will have the same order, that is
//  0,  1,  2,  3
//  4,  5,  6,  7
//  8,  9, 10, 11
// 12, 13, 14, 15

namespace terrain_mesh_index_generator {

// generates a triangle strip for the surface.
// i.e., for the example 0, 4, 1, 5, 2, 6, 3, 7, 7, 11, 6, 10, 5, 9, 4, 8, 8, 12, ..
// triangles (3, 7, 7), (7, 7, 11), (4, 8, 9), and (8, 8, 12) are degenerate intentionally.
// this keeps the strip running, and shouldn't be visible.
template <typename Index>
std::vector<Index> surface_quads(unsigned vertex_side_length) {
  assert(vertex_side_length >= 2);
  std::vector<Index> indices;
  const auto height = vertex_side_length;
  const auto width = vertex_side_length;
  for (size_t row = 0; row < height - 1; row++) {
    const bool left2right = (row & 1) == 0;
    const size_t start = left2right ? 0 : width - 1;
    for (size_t col = start; col < width; left2right ? col++ : col--) {
      indices.push_back(col + row * width);
      indices.push_back(col + (row + 1) * width);
    }
  }
  return indices;
}
}

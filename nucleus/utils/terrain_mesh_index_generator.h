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

#include <cassert>
#include <limits>
#include <vector>

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

namespace nucleus::utils::terrain_mesh_index_generator {

// generates a triangle strip for the surface. this corrected version should respect the winding order
// i.e., for the example 0, 4, 1, 5, 2, 6, 3, 7, 7, 4, 4, 8, 5, 9, 6, 10, 7, 11, 11, 8, 8, ..
// triangles (3, 7, 7), (7, 7, 4), (4, 4, 8), (11, 11, 8) etc are degenerate intentionally.
// this keeps the strip running, and shouldn't be visible.
template<typename Index>
std::vector<Index> surface_quads(unsigned vertex_side_length)
{
    assert(vertex_side_length >= 2);
    assert(vertex_side_length * vertex_side_length < std::numeric_limits<Index>::max());
    std::vector<Index> indices;
    const auto height = vertex_side_length;
    const auto width = vertex_side_length;

    const auto index_for = [&width](auto row, auto col) { return col + row * width; };

    for (size_t row = 0; row < height - 1; row++) {
        for (size_t col = 0; col < width; col++) {
            indices.push_back(index_for(row, col));
            indices.push_back(index_for(row + 1, col));
        }
        indices.push_back(index_for(row + 1, width - 1));
        indices.push_back(index_for(row + 1, 0));
    }
    indices.resize(indices.size() - 2);
    return indices;
}

template<typename Index>
std::vector<Index> surface_quads_with_curtains(unsigned vertex_side_length)
{
    assert(vertex_side_length >= 2);
    assert(vertex_side_length * vertex_side_length < std::numeric_limits<Index>::max());
    std::vector<Index> indices = surface_quads<Index>(vertex_side_length);
    const auto height = vertex_side_length;
    const auto width = vertex_side_length;
    const auto index_for = [&width](auto row, auto col) { return col + row * width; };
    auto curtain_index = indices.back() + 1;
    const auto first_curtain_index = curtain_index;

    for (size_t row = height - 1; row >= 1; row--) {
        indices.push_back(index_for(row, width - 1));
        indices.push_back(curtain_index++);
    }

    for (size_t col = width - 1; col >= 1; col--) {
        indices.push_back(index_for(0, col));
        indices.push_back(curtain_index++);
    }

    for (size_t row = 0; row < height - 1; row++) {
        indices.push_back(index_for(row, 0));
        indices.push_back(curtain_index++);
    }

    for (size_t col = 0; col < width - 1; col++) {
        indices.push_back(index_for(height - 1, col));
        indices.push_back(curtain_index++);
    }
    indices.push_back(index_for(height - 1, width - 1));
    indices.push_back(first_curtain_index);

    return indices;
}
}

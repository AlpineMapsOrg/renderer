/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adam Celarek
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

#include "types.h"
#include "utils.h"
#include <nucleus/camera/Definition.h>

namespace nucleus::tile::drawing {
constexpr uint max_n_tiles = 1024;

std::vector<TileBounds> generate_list(const camera::Definition& camera, utils::AabbDecoratorPtr aabb_decorator, unsigned max_zoom_level);
std::vector<TileBounds> sort(std::vector<TileBounds> list, const glm::dvec3& camera_position);
}

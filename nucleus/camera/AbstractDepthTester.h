/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Adam Celarek
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

#include <glm/glm.hpp>

namespace nucleus::camera {
class Definition;

class AbstractDepthTester {
public:
    [[nodiscard]] virtual float depth(const glm::dvec2& normalised_device_coordinates) = 0;
    [[nodiscard]] virtual glm::dvec3 position(const glm::dvec2& normalised_device_coordinates) = 0;

    //TODO implement this for other directions
    //[[nodiscard]] virtual glm::dvec3 ray_cast(const Definition& camera, const glm::dvec2& normalised_device_coordinates) = 0;
};
}

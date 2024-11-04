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

#include <optional>

#include "Definition.h"

namespace nucleus::camera {
class AbstractDepthTester;

class AnimationStyle
{
public:
    virtual ~AnimationStyle() = default;
    virtual std::optional<Definition> update(Definition camera, AbstractDepthTester* depth_tester);
    virtual std::optional<glm::vec2> operation_centre();
    virtual std::optional<float> operation_centre_distance(Definition camera);
};

} // namespace nucleus::camera

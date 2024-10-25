/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#include "AnimationStyle.h"
#include "nucleus/utils/Stopwatch.h"

namespace nucleus::camera {
class RotateNorthAnimation : public AnimationStyle
{
    glm::dvec3 m_operation_centre = {};
    utils::Stopwatch m_stopwatch = {};
    float m_degrees_from_north = 0;
    int m_total_duration = 1000;
    int m_current_duration = 0;
public:
    RotateNorthAnimation(Definition camera, AbstractDepthTester* depth_tester);
    std::optional<Definition> update(Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<glm::vec2> operation_centre() override;
private:
    float ease_in_out(float t);
    glm::vec2 m_operation_centre_screen = {};
};
}

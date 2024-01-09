/*****************************************************************************
 * Alpine Terrain Renderer
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

#include "InteractionStyle.h"
#include "nucleus/utils/Stopwatch.h"

namespace nucleus::camera {

class CadInteraction : public InteractionStyle
{
    glm::dvec3 m_operation_centre = {};
    glm::vec2 m_operation_centre_screen = {};
    utils::Stopwatch m_stopwatch = {};
    glm::dvec3 m_interpolation_start = {};
    glm::dvec3 m_interpolation_target = {};
    int m_interpolation_duration = 120;
    bool m_key_ctrl = false;
    bool m_key_alt = false;
public:
    void reset_interaction(Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> mouse_press_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> mouse_move_event(const event_parameter::Mouse& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> wheel_event(const event_parameter::Wheel& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> key_press_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> key_release_event(const QKeyCombination& e, Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<Definition> update(Definition camera, AbstractDepthTester* depth_tester) override;
    std::optional<glm::vec2> operation_centre() override;
    std::optional<float> operation_centre_distance(Definition camera) override;
};
}
